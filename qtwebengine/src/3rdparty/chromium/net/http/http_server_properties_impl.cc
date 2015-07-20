// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace net {

namespace {

const uint64 kBrokenAlternateProtocolDelaySecs = 300;

}  // namespace

HttpServerPropertiesImpl::HttpServerPropertiesImpl()
    : spdy_servers_map_(SpdyServerHostPortMap::NO_AUTO_EVICT),
      alternate_protocol_map_(AlternateProtocolMap::NO_AUTO_EVICT),
      spdy_settings_map_(SpdySettingsMap::NO_AUTO_EVICT),
      alternate_protocol_probability_threshold_(1),
      weak_ptr_factory_(this) {
  canonical_suffixes_.push_back(".c.youtube.com");
  canonical_suffixes_.push_back(".googlevideo.com");
  canonical_suffixes_.push_back(".googleusercontent.com");
}

HttpServerPropertiesImpl::~HttpServerPropertiesImpl() {
}

void HttpServerPropertiesImpl::InitializeSpdyServers(
    std::vector<std::string>* spdy_servers,
    bool support_spdy) {
  DCHECK(CalledOnValidThread());
  if (!spdy_servers)
    return;
  // Add the entries from persisted data.
  for (std::vector<std::string>::reverse_iterator it = spdy_servers->rbegin();
       it != spdy_servers->rend(); ++it) {
    spdy_servers_map_.Put(*it, support_spdy);
  }
}

void HttpServerPropertiesImpl::InitializeAlternateProtocolServers(
    AlternateProtocolMap* alternate_protocol_map) {
  // Keep all the broken ones since those don't get persisted.
  for (AlternateProtocolMap::iterator it = alternate_protocol_map_.begin();
       it != alternate_protocol_map_.end();) {
    AlternateProtocolMap::iterator old_it = it;
    ++it;
    if (!old_it->second.is_broken) {
      alternate_protocol_map_.Erase(old_it);
    }
  }

  // Add the entries from persisted data.
  for (AlternateProtocolMap::reverse_iterator it =
           alternate_protocol_map->rbegin();
       it != alternate_protocol_map->rend(); ++it) {
    alternate_protocol_map_.Put(it->first, it->second);
  }

  // Attempt to find canonical servers.
  int canonical_ports[] = { 80, 443 };
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    for (size_t j = 0; j < arraysize(canonical_ports); ++j) {
      HostPortPair canonical_host(canonical_suffix, canonical_ports[j]);
      // If we already have a valid canonical server, we're done.
      if (ContainsKey(canonical_host_to_origin_map_, canonical_host) &&
          (alternate_protocol_map_.Peek(canonical_host_to_origin_map_[
               canonical_host]) != alternate_protocol_map_.end())) {
        continue;
      }
      // Now attempt to find a server which matches this origin and set it as
      // canonical .
      for (AlternateProtocolMap::const_iterator it =
               alternate_protocol_map_.begin();
           it != alternate_protocol_map_.end(); ++it) {
        if (EndsWith(it->first.host(), canonical_suffixes_[i], false)) {
          canonical_host_to_origin_map_[canonical_host] = it->first;
          break;
        }
      }
    }
  }
}

void HttpServerPropertiesImpl::InitializeSpdySettingsServers(
    SpdySettingsMap* spdy_settings_map) {
  for (SpdySettingsMap::reverse_iterator it = spdy_settings_map->rbegin();
       it != spdy_settings_map->rend(); ++it) {
    spdy_settings_map_.Put(it->first, it->second);
  }
}

void HttpServerPropertiesImpl::InitializeSupportsQuic(
    SupportsQuicMap* supports_quic_map) {
  for (SupportsQuicMap::reverse_iterator it = supports_quic_map->rbegin();
       it != supports_quic_map->rend();
       ++it) {
    supports_quic_map_.insert(std::make_pair(it->first, it->second));
  }
}

void HttpServerPropertiesImpl::GetSpdyServerList(
    base::ListValue* spdy_server_list,
    size_t max_size) const {
  DCHECK(CalledOnValidThread());
  DCHECK(spdy_server_list);
  spdy_server_list->Clear();
  size_t count = 0;
  // Get the list of servers (host/port) that support SPDY.
  for (SpdyServerHostPortMap::const_iterator it = spdy_servers_map_.begin();
       it != spdy_servers_map_.end() && count < max_size; ++it) {
    const std::string spdy_server_host_port = it->first;
    if (it->second) {
      spdy_server_list->Append(new base::StringValue(spdy_server_host_port));
      ++count;
    }
  }
}

// static
std::string HttpServerPropertiesImpl::GetFlattenedSpdyServer(
    const net::HostPortPair& host_port_pair) {
  std::string spdy_server;
  spdy_server.append(host_port_pair.host());
  spdy_server.append(":");
  base::StringAppendF(&spdy_server, "%d", host_port_pair.port());
  return spdy_server;
}

static const AlternateProtocolInfo* g_forced_alternate_protocol = NULL;

// static
void HttpServerPropertiesImpl::ForceAlternateProtocol(
    const AlternateProtocolInfo& info) {
  // Note: we're going to leak this.
  if (g_forced_alternate_protocol)
    delete g_forced_alternate_protocol;
  g_forced_alternate_protocol = new AlternateProtocolInfo(info);
}

// static
void HttpServerPropertiesImpl::DisableForcedAlternateProtocol() {
  delete g_forced_alternate_protocol;
  g_forced_alternate_protocol = NULL;
}

base::WeakPtr<HttpServerProperties> HttpServerPropertiesImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HttpServerPropertiesImpl::Clear() {
  DCHECK(CalledOnValidThread());
  spdy_servers_map_.Clear();
  alternate_protocol_map_.Clear();
  canonical_host_to_origin_map_.clear();
  spdy_settings_map_.Clear();
  supports_quic_map_.clear();
}

bool HttpServerPropertiesImpl::SupportsSpdy(
    const net::HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;
  std::string spdy_server = GetFlattenedSpdyServer(host_port_pair);

  SpdyServerHostPortMap::iterator spdy_host_port =
      spdy_servers_map_.Get(spdy_server);
  if (spdy_host_port != spdy_servers_map_.end())
    return spdy_host_port->second;
  return false;
}

void HttpServerPropertiesImpl::SetSupportsSpdy(
    const net::HostPortPair& host_port_pair,
    bool support_spdy) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return;
  std::string spdy_server = GetFlattenedSpdyServer(host_port_pair);

  SpdyServerHostPortMap::iterator spdy_host_port =
      spdy_servers_map_.Get(spdy_server);
  if ((spdy_host_port != spdy_servers_map_.end()) &&
      (spdy_host_port->second == support_spdy)) {
    return;
  }
  // Cache the data.
  spdy_servers_map_.Put(spdy_server, support_spdy);
}

bool HttpServerPropertiesImpl::HasAlternateProtocol(
    const HostPortPair& server) {
  if (g_forced_alternate_protocol)
    return true;
  AlternateProtocolMap::const_iterator it = alternate_protocol_map_.Get(server);
  if (it != alternate_protocol_map_.end())
    return it->second.probability >= alternate_protocol_probability_threshold_;

  auto canonical = GetCanonicalHost(server);
  if (canonical == canonical_host_to_origin_map_.end() ||
      canonical->second.Equals(server)) {
    return false;
  }

  return HasAlternateProtocol(canonical->second);
}

std::string HttpServerPropertiesImpl::GetCanonicalSuffix(
    const HostPortPair& server) {
  // If this host ends with a canonical suffix, then return the canonical
  // suffix.
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (EndsWith(server.host(), canonical_suffixes_[i], false)) {
      return canonical_suffix;
    }
  }
  return std::string();
}

AlternateProtocolInfo
HttpServerPropertiesImpl::GetAlternateProtocol(
    const HostPortPair& server) {
  DCHECK(HasAlternateProtocol(server));

  // First check the map.
  AlternateProtocolMap::iterator it = alternate_protocol_map_.Get(server);
  if (it != alternate_protocol_map_.end())
    return it->second;

  // Next check the canonical host.
  CanonicalHostMap::const_iterator canonical_host = GetCanonicalHost(server);
  if (canonical_host != canonical_host_to_origin_map_.end())
    return alternate_protocol_map_.Get(canonical_host->second)->second;

  // We must be forcing an alternate.
  DCHECK(g_forced_alternate_protocol);
  return *g_forced_alternate_protocol;
}

void HttpServerPropertiesImpl::SetAlternateProtocol(
    const HostPortPair& server,
    uint16 alternate_port,
    AlternateProtocol alternate_protocol,
    double alternate_probability) {

  AlternateProtocolInfo alternate(alternate_port,
                                  alternate_protocol,
                                  alternate_probability);
  if (HasAlternateProtocol(server)) {
    const AlternateProtocolInfo existing_alternate =
        GetAlternateProtocol(server);

    if (existing_alternate.is_broken) {
      DVLOG(1) << "Ignore alternate protocol since it's known to be broken.";
      return;
    }

    if (!existing_alternate.Equals(alternate)) {
      LOG(WARNING) << "Changing the alternate protocol for: "
                   << server.ToString()
                   << " from [Port: " << existing_alternate.port
                   << ", Protocol: " << existing_alternate.protocol
                   << ", Probability: " << existing_alternate.probability
                   << "] to [Port: " << alternate_port
                   << ", Protocol: " << alternate_protocol
                   << ", Probability: " << alternate_probability
                   << "].";
    }
  } else {
    if (alternate_probability >= alternate_protocol_probability_threshold_) {
      // TODO(rch): Consider the case where multiple requests are started
      // before the first completes. In this case, only one of the jobs
      // would reach this code, whereas all of them should should have.
      HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING);
    }
  }

  alternate_protocol_map_.Put(server, alternate);

  // If this host ends with a canonical suffix, then set it as the
  // canonical host.
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (EndsWith(server.host(), canonical_suffixes_[i], false)) {
      HostPortPair canonical_host(canonical_suffix, server.port());
      canonical_host_to_origin_map_[canonical_host] = server;
      break;
    }
  }
}

void HttpServerPropertiesImpl::SetBrokenAlternateProtocol(
    const HostPortPair& server) {
  AlternateProtocolMap::iterator it = alternate_protocol_map_.Get(server);
  if (it == alternate_protocol_map_.end()) {
    LOG(DFATAL) << "Trying to mark unknown alternate protocol broken.";
    return;
  }
  it->second.is_broken = true;
  int count = ++broken_alternate_protocol_map_[server];
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(kBrokenAlternateProtocolDelaySecs);
  BrokenAlternateProtocolEntry entry;
  entry.server = server;
  entry.when = base::TimeTicks::Now() + delay * (1 << (count - 1));
  broken_alternate_protocol_list_.push_back(entry);

  // Do not leave this host as canonical so that we don't infer the other
  // hosts are also broken without testing them first.
  RemoveCanonicalHost(server);

  // If this is the only entry in the list, schedule an expiration task.
  // Otherwse it will be rescheduled automatically when the pending
  // task runs.
  if (broken_alternate_protocol_list_.size() == 1) {
    ScheduleBrokenAlternateProtocolMappingsExpiration();
  }
}

bool HttpServerPropertiesImpl::WasAlternateProtocolRecentlyBroken(
    const HostPortPair& server) {
  return ContainsKey(broken_alternate_protocol_map_, server);
}

void HttpServerPropertiesImpl::ConfirmAlternateProtocol(
    const HostPortPair& server) {
  broken_alternate_protocol_map_.erase(server);
}

void HttpServerPropertiesImpl::ClearAlternateProtocol(
    const HostPortPair& server) {
  AlternateProtocolMap::iterator it = alternate_protocol_map_.Peek(server);
  if (it != alternate_protocol_map_.end())
    alternate_protocol_map_.Erase(it);

  RemoveCanonicalHost(server);
}

const AlternateProtocolMap&
HttpServerPropertiesImpl::alternate_protocol_map() const {
  return alternate_protocol_map_;
}

const SettingsMap& HttpServerPropertiesImpl::GetSpdySettings(
    const HostPortPair& host_port_pair) {
  SpdySettingsMap::iterator it = spdy_settings_map_.Get(host_port_pair);
  if (it == spdy_settings_map_.end()) {
    CR_DEFINE_STATIC_LOCAL(SettingsMap, kEmptySettingsMap, ());
    return kEmptySettingsMap;
  }
  return it->second;
}

bool HttpServerPropertiesImpl::SetSpdySetting(
    const HostPortPair& host_port_pair,
    SpdySettingsIds id,
    SpdySettingsFlags flags,
    uint32 value) {
  if (!(flags & SETTINGS_FLAG_PLEASE_PERSIST))
      return false;

  SettingsFlagsAndValue flags_and_value(SETTINGS_FLAG_PERSISTED, value);
  SpdySettingsMap::iterator it = spdy_settings_map_.Get(host_port_pair);
  if (it == spdy_settings_map_.end()) {
    SettingsMap settings_map;
    settings_map[id] = flags_and_value;
    spdy_settings_map_.Put(host_port_pair, settings_map);
  } else {
    SettingsMap& settings_map = it->second;
    settings_map[id] = flags_and_value;
  }
  return true;
}

void HttpServerPropertiesImpl::ClearSpdySettings(
    const HostPortPair& host_port_pair) {
  SpdySettingsMap::iterator it = spdy_settings_map_.Peek(host_port_pair);
  if (it != spdy_settings_map_.end())
    spdy_settings_map_.Erase(it);
}

void HttpServerPropertiesImpl::ClearAllSpdySettings() {
  spdy_settings_map_.Clear();
}

const SpdySettingsMap&
HttpServerPropertiesImpl::spdy_settings_map() const {
  return spdy_settings_map_;
}

SupportsQuic HttpServerPropertiesImpl::GetSupportsQuic(
    const HostPortPair& host_port_pair) const {
  SupportsQuicMap::const_iterator it = supports_quic_map_.find(host_port_pair);
  if (it == supports_quic_map_.end()) {
    CR_DEFINE_STATIC_LOCAL(SupportsQuic, kEmptySupportsQuic, ());
    return kEmptySupportsQuic;
  }
  return it->second;
}

void HttpServerPropertiesImpl::SetSupportsQuic(
    const HostPortPair& host_port_pair,
    bool used_quic,
    const std::string& address) {
  SupportsQuic supports_quic(used_quic, address);
  supports_quic_map_.insert(std::make_pair(host_port_pair, supports_quic));
}

const SupportsQuicMap&
HttpServerPropertiesImpl::supports_quic_map() const {
  return supports_quic_map_;
}

void HttpServerPropertiesImpl::SetServerNetworkStats(
    const HostPortPair& host_port_pair,
    NetworkStats stats) {
  server_network_stats_map_[host_port_pair] = stats;
}

const HttpServerProperties::NetworkStats*
HttpServerPropertiesImpl::GetServerNetworkStats(
    const HostPortPair& host_port_pair) const {
  ServerNetworkStatsMap::const_iterator it =
      server_network_stats_map_.find(host_port_pair);
  if (it == server_network_stats_map_.end()) {
    return NULL;
  }
  return &it->second;
}

void HttpServerPropertiesImpl::SetAlternateProtocolProbabilityThreshold(
    double threshold) {
  alternate_protocol_probability_threshold_ = threshold;
}

HttpServerPropertiesImpl::CanonicalHostMap::const_iterator
HttpServerPropertiesImpl::GetCanonicalHost(HostPortPair server) const {
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (EndsWith(server.host(), canonical_suffixes_[i], false)) {
      HostPortPair canonical_host(canonical_suffix, server.port());
      return canonical_host_to_origin_map_.find(canonical_host);
    }
  }

  return canonical_host_to_origin_map_.end();
}

void HttpServerPropertiesImpl::RemoveCanonicalHost(
    const HostPortPair& server) {
  CanonicalHostMap::const_iterator canonical = GetCanonicalHost(server);
  if (canonical == canonical_host_to_origin_map_.end())
    return;

  if (!canonical->second.Equals(server))
    return;

  canonical_host_to_origin_map_.erase(canonical->first);
}

void HttpServerPropertiesImpl::ExpireBrokenAlternateProtocolMappings() {
  base::TimeTicks now = base::TimeTicks::Now();
  while (!broken_alternate_protocol_list_.empty()) {
    BrokenAlternateProtocolEntry entry =
        broken_alternate_protocol_list_.front();
    if (now < entry.when) {
      break;
    }

    ClearAlternateProtocol(entry.server);
    broken_alternate_protocol_list_.pop_front();
  }
  ScheduleBrokenAlternateProtocolMappingsExpiration();
}

void
HttpServerPropertiesImpl::ScheduleBrokenAlternateProtocolMappingsExpiration() {
  if (broken_alternate_protocol_list_.empty()) {
    return;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks when = broken_alternate_protocol_list_.front().when;
  base::TimeDelta delay = when > now ? when - now : base::TimeDelta();
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &HttpServerPropertiesImpl::ExpireBrokenAlternateProtocolMappings,
          weak_ptr_factory_.GetWeakPtr()),
      delay);
}

}  // namespace net
