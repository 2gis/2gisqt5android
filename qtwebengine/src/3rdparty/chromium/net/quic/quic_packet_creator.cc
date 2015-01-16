// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::vector;

namespace net {

// A QuicRandom wrapper that gets a bucket of entropy and distributes it
// bit-by-bit. Replenishes the bucket as needed. Not thread-safe. Expose this
// class if single bit randomness is needed elsewhere.
class QuicRandomBoolSource {
 public:
  // random: Source of entropy. Not owned.
  explicit QuicRandomBoolSource(QuicRandom* random)
      : random_(random),
        bit_bucket_(0),
        bit_mask_(0) {}

  ~QuicRandomBoolSource() {}

  // Returns the next random bit from the bucket.
  bool RandBool() {
    if (bit_mask_ == 0) {
      bit_bucket_ = random_->RandUint64();
      bit_mask_ = 1;
    }
    bool result = ((bit_bucket_ & bit_mask_) != 0);
    bit_mask_ <<= 1;
    return result;
  }

 private:
  // Source of entropy.
  QuicRandom* random_;
  // Stored random bits.
  uint64 bit_bucket_;
  // The next available bit has "1" in the mask. Zero means empty bucket.
  uint64 bit_mask_;

  DISALLOW_COPY_AND_ASSIGN(QuicRandomBoolSource);
};

QuicPacketCreator::QuicPacketCreator(QuicConnectionId connection_id,
                                     QuicFramer* framer,
                                     QuicRandom* random_generator)
    : connection_id_(connection_id),
      encryption_level_(ENCRYPTION_NONE),
      framer_(framer),
      random_bool_source_(new QuicRandomBoolSource(random_generator)),
      sequence_number_(0),
      should_fec_protect_(false),
      fec_group_number_(0),
      send_version_in_packet_(!framer->is_server()),
      max_packet_length_(kDefaultMaxPacketSize),
      max_packets_per_fec_group_(0),
      connection_id_length_(PACKET_8BYTE_CONNECTION_ID),
      next_sequence_number_length_(PACKET_1BYTE_SEQUENCE_NUMBER),
      sequence_number_length_(next_sequence_number_length_),
      packet_size_(0) {
  framer_->set_fec_builder(this);
}

QuicPacketCreator::~QuicPacketCreator() {
}

void QuicPacketCreator::OnBuiltFecProtectedPayload(
    const QuicPacketHeader& header, StringPiece payload) {
  if (fec_group_.get()) {
    DCHECK_NE(0u, header.fec_group);
    fec_group_->Update(encryption_level_, header, payload);
  }
}

bool QuicPacketCreator::ShouldSendFec(bool force_close) const {
  DCHECK(!HasPendingFrames());
  return fec_group_.get() != NULL && fec_group_->NumReceivedPackets() > 0 &&
      (force_close || fec_group_->NumReceivedPackets() >=
                      max_packets_per_fec_group_);
}

bool QuicPacketCreator::IsFecGroupOpen() const {
  return ShouldSendFec(true);
}

void QuicPacketCreator::StartFecProtectingPackets() {
  if (!IsFecEnabled()) {
    LOG(DFATAL) << "Cannot start FEC protection when FEC is not enabled.";
    return;
  }
  // TODO(jri): This currently requires that the generator flush out any
  // pending frames when FEC protection is turned on. If current packet can be
  // converted to an FEC protected packet, do it. This will require the
  // generator to check if the resulting expansion still allows the incoming
  // frame to be added to the packet.
  if (HasPendingFrames()) {
    LOG(DFATAL) << "Cannot start FEC protection with pending frames.";
    return;
  }
  DCHECK(!should_fec_protect_);
  should_fec_protect_ = true;
}

void QuicPacketCreator::StopFecProtectingPackets() {
  if (fec_group_.get() != NULL) {
    LOG(DFATAL) << "Cannot stop FEC protection with open FEC group.";
    return;
  }
  DCHECK(should_fec_protect_);
  should_fec_protect_ = false;
  fec_group_number_ = 0;
}

bool QuicPacketCreator::IsFecProtected() const {
  return should_fec_protect_;
}

bool QuicPacketCreator::IsFecEnabled() const {
  return max_packets_per_fec_group_ > 0;
}

InFecGroup QuicPacketCreator::MaybeUpdateLengthsAndStartFec() {
  if (fec_group_.get() != NULL) {
    // Don't update any lengths when an FEC group is open, to ensure same
    // packet header size in all packets within a group.
    return IN_FEC_GROUP;
  }
  if (!queued_frames_.empty()) {
    // Don't change creator state if there are frames queued.
    return fec_group_.get() == NULL ? NOT_IN_FEC_GROUP : IN_FEC_GROUP;
  }

  // Update sequence number length only on packet and FEC group boundaries.
  sequence_number_length_ = next_sequence_number_length_;

  if (!should_fec_protect_) {
    return NOT_IN_FEC_GROUP;
  }
  // Start a new FEC group since protection is on. Set the fec group number to
  // the sequence number of the next packet.
  fec_group_number_ = sequence_number() + 1;
  fec_group_.reset(new QuicFecGroup());
  return IN_FEC_GROUP;
}

// Stops serializing version of the protocol in packets sent after this call.
// A packet that is already open might send kQuicVersionSize bytes less than the
// maximum packet size if we stop sending version before it is serialized.
void QuicPacketCreator::StopSendingVersion() {
  DCHECK(send_version_in_packet_);
  send_version_in_packet_ = false;
  if (packet_size_ > 0) {
    DCHECK_LT(kQuicVersionSize, packet_size_);
    packet_size_ -= kQuicVersionSize;
  }
}

void QuicPacketCreator::UpdateSequenceNumberLength(
      QuicPacketSequenceNumber least_packet_awaited_by_peer,
      QuicByteCount congestion_window) {
  DCHECK_LE(least_packet_awaited_by_peer, sequence_number_ + 1);
  // Since the packet creator will not change sequence number length mid FEC
  // group, include the size of an FEC group to be safe.
  const QuicPacketSequenceNumber current_delta =
      max_packets_per_fec_group_ + sequence_number_ + 1
      - least_packet_awaited_by_peer;
  const uint64 congestion_window_packets =
      congestion_window / max_packet_length_;
  const uint64 delta = max(current_delta, congestion_window_packets);
  next_sequence_number_length_ =
      QuicFramer::GetMinSequenceNumberLength(delta * 4);
}

bool QuicPacketCreator::HasRoomForStreamFrame(QuicStreamId id,
                                              QuicStreamOffset offset) const {
  // TODO(jri): This is a simple safe decision for now, but make
  // is_in_fec_group a parameter. Same as with all public methods in
  // QuicPacketCreator.
  return BytesFree() >
      QuicFramer::GetMinStreamFrameSize(framer_->version(), id, offset, true,
                                        should_fec_protect_ ? IN_FEC_GROUP :
                                                              NOT_IN_FEC_GROUP);
}

// static
size_t QuicPacketCreator::StreamFramePacketOverhead(
    QuicVersion version,
    QuicConnectionIdLength connection_id_length,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length,
    QuicStreamOffset offset,
    InFecGroup is_in_fec_group) {
  return GetPacketHeaderSize(connection_id_length, include_version,
                             sequence_number_length, is_in_fec_group) +
      // Assumes this is a stream with a single lone packet.
      QuicFramer::GetMinStreamFrameSize(version, 1u, offset, true,
                                        is_in_fec_group);
}

size_t QuicPacketCreator::CreateStreamFrame(QuicStreamId id,
                                            const IOVector& data,
                                            QuicStreamOffset offset,
                                            bool fin,
                                            QuicFrame* frame) {
  DCHECK_GT(max_packet_length_, StreamFramePacketOverhead(
                framer_->version(), PACKET_8BYTE_CONNECTION_ID, kIncludeVersion,
                PACKET_6BYTE_SEQUENCE_NUMBER, offset, IN_FEC_GROUP));

  InFecGroup is_in_fec_group = MaybeUpdateLengthsAndStartFec();

  LOG_IF(DFATAL, !HasRoomForStreamFrame(id, offset))
      << "No room for Stream frame, BytesFree: " << BytesFree()
      << " MinStreamFrameSize: "
      << QuicFramer::GetMinStreamFrameSize(
          framer_->version(), id, offset, true, is_in_fec_group);

  if (data.Empty()) {
    LOG_IF(DFATAL, !fin)
        << "Creating a stream frame with no data or fin.";
    // Create a new packet for the fin, if necessary.
    *frame = QuicFrame(new QuicStreamFrame(id, true, offset, data));
    return 0;
  }

  const size_t data_size = data.TotalBufferSize();
  size_t min_frame_size = QuicFramer::GetMinStreamFrameSize(
      framer_->version(), id, offset, /*last_frame_in_packet=*/ true,
      is_in_fec_group);
  size_t bytes_consumed = min<size_t>(BytesFree() - min_frame_size, data_size);

  bool set_fin = fin && bytes_consumed == data_size;  // Last frame.
  IOVector frame_data;
  frame_data.AppendIovecAtMostBytes(data.iovec(), data.Size(),
                                    bytes_consumed);
  DCHECK_EQ(frame_data.TotalBufferSize(), bytes_consumed);
  *frame = QuicFrame(new QuicStreamFrame(id, set_fin, offset, frame_data));
  return bytes_consumed;
}

size_t QuicPacketCreator::CreateStreamFrameWithNotifier(
    QuicStreamId id,
    const IOVector& data,
    QuicStreamOffset offset,
    bool fin,
    QuicAckNotifier* notifier,
    QuicFrame* frame) {
  size_t bytes_consumed = CreateStreamFrame(id, data, offset, fin, frame);

  // The frame keeps track of the QuicAckNotifier until it is serialized into
  // a packet. At that point the notifier is informed of the sequence number
  // of the packet that this frame was eventually sent in.
  frame->stream_frame->notifier = notifier;

  return bytes_consumed;
}

SerializedPacket QuicPacketCreator::ReserializeAllFrames(
    const QuicFrames& frames,
    QuicSequenceNumberLength original_length) {
  DCHECK(fec_group_.get() == NULL);
  const QuicSequenceNumberLength saved_length = sequence_number_length_;
  const QuicSequenceNumberLength saved_next_length =
      next_sequence_number_length_;
  const bool saved_should_fec_protect = should_fec_protect_;

  // Temporarily set the sequence number length and stop FEC protection.
  sequence_number_length_ = original_length;
  next_sequence_number_length_ = original_length;
  should_fec_protect_ = false;

  // Serialize the packet and restore the FEC and sequence number length state.
  SerializedPacket serialized_packet = SerializeAllFrames(frames);
  sequence_number_length_ = saved_length;
  next_sequence_number_length_ = saved_next_length;
  should_fec_protect_ = saved_should_fec_protect;

  return serialized_packet;
}

SerializedPacket QuicPacketCreator::SerializeAllFrames(
    const QuicFrames& frames) {
  // TODO(satyamshekhar): Verify that this DCHECK won't fail. What about queued
  // frames from SendStreamData()[send_stream_should_flush_ == false &&
  // data.empty() == true] and retransmit due to RTO.
  DCHECK_EQ(0u, queued_frames_.size());
  LOG_IF(DFATAL, frames.empty())
      << "Attempt to serialize empty packet";
  for (size_t i = 0; i < frames.size(); ++i) {
    bool success = AddFrame(frames[i], false);
    DCHECK(success);
  }
  SerializedPacket packet = SerializePacket();
  DCHECK(packet.retransmittable_frames == NULL);
  return packet;
}

bool QuicPacketCreator::HasPendingFrames() const {
  return !queued_frames_.empty();
}

bool QuicPacketCreator::HasPendingRetransmittableFrames() const {
  return queued_retransmittable_frames_.get() != NULL &&
      !queued_retransmittable_frames_->frames().empty();
}

size_t QuicPacketCreator::ExpansionOnNewFrame() const {
  // If packet is FEC protected, there's no expansion.
  if (should_fec_protect_) {
      return 0;
  }
  // If the last frame in the packet is a stream frame, then it will expand to
  // include the stream_length field when a new frame is added.
  bool has_trailing_stream_frame =
      !queued_frames_.empty() && queued_frames_.back().type == STREAM_FRAME;
  return has_trailing_stream_frame ? kQuicStreamPayloadLengthSize : 0;
}

size_t QuicPacketCreator::BytesFree() const {
  const size_t max_plaintext_size =
      framer_->GetMaxPlaintextSize(max_packet_length_);
  DCHECK_GE(max_plaintext_size, PacketSize());
  return max_plaintext_size - min(max_plaintext_size, PacketSize()
                                  + ExpansionOnNewFrame());
}

size_t QuicPacketCreator::PacketSize() const {
  if (!queued_frames_.empty()) {
    return packet_size_;
  }
  if (fec_group_.get() == NULL) {
    // Update sequence number length on packet and FEC boundary.
    sequence_number_length_ = next_sequence_number_length_;
  }
  packet_size_ = GetPacketHeaderSize(
      connection_id_length_, send_version_in_packet_, sequence_number_length_,
      should_fec_protect_ ? IN_FEC_GROUP : NOT_IN_FEC_GROUP);
  return packet_size_;
}

bool QuicPacketCreator::AddSavedFrame(const QuicFrame& frame) {
  return AddFrame(frame, true);
}

SerializedPacket QuicPacketCreator::SerializePacket() {
  LOG_IF(DFATAL, queued_frames_.empty())
      << "Attempt to serialize empty packet";
  DCHECK_GE(sequence_number_ + 1, fec_group_number_);
  QuicPacketHeader header;
  FillPacketHeader(should_fec_protect_ ? fec_group_number_ : 0, false, &header);

  MaybeAddPadding();

  size_t max_plaintext_size =
      framer_->GetMaxPlaintextSize(max_packet_length_);
  DCHECK_GE(max_plaintext_size, packet_size_);
  // ACK Frames will be truncated only if they're the only frame in the packet,
  // and if packet_size_ was set to max_plaintext_size. If truncation occurred,
  // then GetSerializedFrameLength will have returned all bytes free.
  bool possibly_truncated = packet_size_ == max_plaintext_size &&
                            queued_frames_.size() == 1 &&
                            queued_frames_.back().type == ACK_FRAME;
  SerializedPacket serialized =
      framer_->BuildDataPacket(header, queued_frames_, packet_size_);
  LOG_IF(DFATAL, !serialized.packet)
      << "Failed to serialize " << queued_frames_.size() << " frames.";
  // Because of possible truncation, we can't be confident that our
  // packet size calculation worked correctly.
  if (!possibly_truncated) {
    DCHECK_EQ(packet_size_, serialized.packet->length());
  }
  packet_size_ = 0;
  queued_frames_.clear();
  serialized.retransmittable_frames = queued_retransmittable_frames_.release();
  return serialized;
}

SerializedPacket QuicPacketCreator::SerializeFec() {
  if (fec_group_.get() == NULL || fec_group_->NumReceivedPackets() <= 0) {
    LOG(DFATAL) << "SerializeFEC called but no group or zero packets in group.";
    // TODO(jri): Make this a public method of framer?
    SerializedPacket kNoPacket(0, PACKET_1BYTE_SEQUENCE_NUMBER, NULL, 0, NULL);
    return kNoPacket;
  }
  DCHECK_EQ(0u, queued_frames_.size());
  QuicPacketHeader header;
  FillPacketHeader(fec_group_number_, true, &header);
  QuicFecData fec_data;
  fec_data.fec_group = fec_group_->min_protected_packet();
  fec_data.redundancy = fec_group_->payload_parity();
  SerializedPacket serialized = framer_->BuildFecPacket(header, fec_data);
  fec_group_.reset(NULL);
  packet_size_ = 0;
  LOG_IF(DFATAL, !serialized.packet)
      << "Failed to serialize fec packet for group:" << fec_data.fec_group;
  DCHECK_GE(max_packet_length_, serialized.packet->length());
  return serialized;
}

SerializedPacket QuicPacketCreator::SerializeConnectionClose(
    QuicConnectionCloseFrame* close_frame) {
  QuicFrames frames;
  frames.push_back(QuicFrame(close_frame));
  return SerializeAllFrames(frames);
}

QuicEncryptedPacket* QuicPacketCreator::SerializeVersionNegotiationPacket(
    const QuicVersionVector& supported_versions) {
  DCHECK(framer_->is_server());
  QuicPacketPublicHeader header;
  header.connection_id = connection_id_;
  header.reset_flag = false;
  header.version_flag = true;
  header.versions = supported_versions;
  QuicEncryptedPacket* encrypted =
      framer_->BuildVersionNegotiationPacket(header, supported_versions);
  DCHECK(encrypted);
  DCHECK_GE(max_packet_length_, encrypted->length());
  return encrypted;
}

void QuicPacketCreator::FillPacketHeader(QuicFecGroupNumber fec_group,
                                         bool fec_flag,
                                         QuicPacketHeader* header) {
  header->public_header.connection_id = connection_id_;
  header->public_header.reset_flag = false;
  header->public_header.version_flag = send_version_in_packet_;
  header->fec_flag = fec_flag;
  header->packet_sequence_number = ++sequence_number_;
  header->public_header.sequence_number_length = sequence_number_length_;
  header->entropy_flag = random_bool_source_->RandBool();
  header->is_in_fec_group = fec_group == 0 ? NOT_IN_FEC_GROUP : IN_FEC_GROUP;
  header->fec_group = fec_group;
}

bool QuicPacketCreator::ShouldRetransmit(const QuicFrame& frame) {
  switch (frame.type) {
    case ACK_FRAME:
    case CONGESTION_FEEDBACK_FRAME:
    case PADDING_FRAME:
    case STOP_WAITING_FRAME:
      return false;
    default:
      return true;
  }
}

bool QuicPacketCreator::AddFrame(const QuicFrame& frame,
                                 bool save_retransmittable_frames) {
  DVLOG(1) << "Adding frame: " << frame;
  InFecGroup is_in_fec_group = MaybeUpdateLengthsAndStartFec();

  size_t frame_len = framer_->GetSerializedFrameLength(
      frame, BytesFree(), queued_frames_.empty(), true, is_in_fec_group,
      sequence_number_length_);
  if (frame_len == 0) {
    return false;
  }
  DCHECK_LT(0u, packet_size_);
  packet_size_ += ExpansionOnNewFrame() + frame_len;

  if (save_retransmittable_frames && ShouldRetransmit(frame)) {
    if (queued_retransmittable_frames_.get() == NULL) {
      queued_retransmittable_frames_.reset(new RetransmittableFrames());
    }
    if (frame.type == STREAM_FRAME) {
      queued_frames_.push_back(
          queued_retransmittable_frames_->AddStreamFrame(frame.stream_frame));
    } else {
      queued_frames_.push_back(
          queued_retransmittable_frames_->AddNonStreamFrame(frame));
    }
  } else {
    queued_frames_.push_back(frame);
  }
  return true;
}

void QuicPacketCreator::MaybeAddPadding() {
  if (BytesFree() == 0) {
    // Don't pad full packets.
    return;
  }

  // If any of the frames in the current packet are on the crypto stream
  // then they contain handshake messagses, and we should pad them.
  bool is_handshake = false;
  for (size_t i = 0; i < queued_frames_.size(); ++i) {
    if (queued_frames_[i].type == STREAM_FRAME &&
        queued_frames_[i].stream_frame->stream_id == kCryptoStreamId) {
      is_handshake = true;
      break;
    }
  }
  if (!is_handshake) {
    return;
  }

  QuicPaddingFrame padding;
  bool success = AddFrame(QuicFrame(&padding), false);
  DCHECK(success);
}

}  // namespace net
