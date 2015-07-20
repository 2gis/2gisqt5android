#include "DMPipeTask.h"
#include "DMUtil.h"
#include "DMWriteTask.h"

#include "SamplePipeControllers.h"
#include "SkCommandLineFlags.h"
#include "SkGPipe.h"

DEFINE_bool(pipe, true, "If true, check several pipe variants against the reference bitmap.");

namespace DM {

static uint32_t get_flags(PipeTask::Mode mode) {
    uint32_t flags = 0;
    if (mode != PipeTask::kInProcess_Mode) {
        flags |= SkGPipeWriter::kCrossProcess_Flag;
    }
    if (mode == PipeTask::kSharedAddress_Mode) {
        flags |= SkGPipeWriter::kSharedAddressSpace_Flag;
    }
    return flags;
}

static const char* get_name(const uint32_t flags) {
    if (flags & SkGPipeWriter::kCrossProcess_Flag &&
        flags & SkGPipeWriter::kSharedAddressSpace_Flag) {
        return "shared-address-space-pipe";
    } else if (flags & SkGPipeWriter::kCrossProcess_Flag) {
        return "cross-process-pipe";
    } else {
        return "pipe";
    }
}

PipeTask::PipeTask(const Task& parent,
                   skiagm::GM* gm,
                   SkBitmap reference,
                   Mode mode)
    : CpuTask(parent)
    , fFlags(get_flags(mode))
    , fName(UnderJoin(parent.name().c_str(), get_name(fFlags)))
    , fGM(gm)
    , fReference(reference)
    {}

void PipeTask::draw() {
    SkBitmap bitmap;
    AllocatePixels(fReference, &bitmap);

    SkCanvas canvas(bitmap);
    PipeController pipeController(&canvas, &SkImageDecoder::DecodeMemory);
    SkGPipeWriter writer;

    SkCanvas* pipeCanvas = writer.startRecording(&pipeController,
                                                 fFlags,
                                                 bitmap.width(),
                                                 bitmap.height());
    CanvasPreflight(pipeCanvas);
    pipeCanvas->concat(fGM->getInitialTransform());
    fGM->draw(pipeCanvas);
    writer.endRecording();

    if (!BitmapsEqual(bitmap, fReference)) {
        this->fail();
        this->spawnChild(SkNEW_ARGS(WriteTask, (*this, "GM", bitmap)));
    }
}

bool PipeTask::shouldSkip() const {
    if (!FLAGS_pipe) {
        return true;
    }
    if (fGM->getFlags() & skiagm::GM::kSkipPipe_Flag) {
        return true;
    }
    if (fFlags == SkGPipeWriter::kCrossProcess_Flag &&
        fGM->getFlags() & skiagm::GM::kSkipPipeCrossProcess_Flag) {
        return true;
    }
    return false;
}

}  // namespace DM
