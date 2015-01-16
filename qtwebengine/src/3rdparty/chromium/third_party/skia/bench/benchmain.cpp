/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "BenchLogger.h"
#include "BenchTimer.h"
#include "Benchmark.h"
#include "CrashHandler.h"
#include "GMBench.h"
#include "ResultsWriter.h"
#include "SkBitmapDevice.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkCommandLineFlags.h"
#include "SkData.h"
#include "SkDeferredCanvas.h"
#include "SkGraphics.h"
#include "SkImageEncoder.h"
#include "SkOSFile.h"
#include "SkPicture.h"
#include "SkPictureRecorder.h"
#include "SkString.h"
#include "SkSurface.h"

#if SK_SUPPORT_GPU
#include "GrContext.h"
#include "GrContextFactory.h"
#include "GrRenderTarget.h"
#include "SkGpuDevice.h"
#include "gl/GrGLDefines.h"
#else
class GrContext;
#endif // SK_SUPPORT_GPU

#include <limits>

enum BenchMode {
    kNormal_BenchMode,
    kDeferred_BenchMode,
    kDeferredSilent_BenchMode,
    kRecord_BenchMode,
    kPictureRecord_BenchMode
};
const char* BenchMode_Name[] = {
    "normal", "deferred", "deferredSilent", "record", "picturerecord"
};

static const char kDefaultsConfigStr[] = "defaults";

///////////////////////////////////////////////////////////////////////////////

class Iter {
public:
    Iter() : fBenches(BenchRegistry::Head()), fGMs(skiagm::GMRegistry::Head()) {}

    Benchmark* next() {
        if (fBenches) {
            BenchRegistry::Factory f = fBenches->factory();
            fBenches = fBenches->next();
            return (*f)(NULL);
        }

        while (fGMs) {
            SkAutoTDelete<skiagm::GM> gm(fGMs->factory()(NULL));
            fGMs = fGMs->next();
            if (gm->getFlags() & skiagm::GM::kAsBench_Flag) {
                return SkNEW_ARGS(GMBench, (gm.detach()));
            }
        }

        return NULL;
    }

private:
    const BenchRegistry* fBenches;
    const skiagm::GMRegistry* fGMs;
};

static void make_filename(const char name[], SkString* path) {
    path->set(name);
    for (int i = 0; name[i]; i++) {
        switch (name[i]) {
            case '/':
            case '\\':
            case ' ':
            case ':':
                path->writable_str()[i] = '-';
                break;
            default:
                break;
        }
    }
}

static void saveFile(const char name[], const char config[], const char dir[],
                     const SkImage* image) {
    SkAutoTUnref<SkData> data(image->encode(SkImageEncoder::kPNG_Type, 100));
    if (NULL == data.get()) {
        return;
    }

    SkString filename;
    make_filename(name, &filename);
    filename.appendf("_%s.png", config);
    SkString path = SkOSPath::SkPathJoin(dir, filename.c_str());
    ::remove(path.c_str());

    SkFILEWStream   stream(path.c_str());
    stream.write(data->data(), data->size());
}

static void perform_clip(SkCanvas* canvas, int w, int h) {
    SkRect r;

    r.set(SkIntToScalar(10), SkIntToScalar(10),
          SkIntToScalar(w*2/3), SkIntToScalar(h*2/3));
    canvas->clipRect(r, SkRegion::kIntersect_Op);

    r.set(SkIntToScalar(w/3), SkIntToScalar(h/3),
          SkIntToScalar(w-10), SkIntToScalar(h-10));
    canvas->clipRect(r, SkRegion::kXOR_Op);
}

static void perform_rotate(SkCanvas* canvas, int w, int h) {
    const SkScalar x = SkIntToScalar(w) / 2;
    const SkScalar y = SkIntToScalar(h) / 2;

    canvas->translate(x, y);
    canvas->rotate(SkIntToScalar(35));
    canvas->translate(-x, -y);
}

static void perform_scale(SkCanvas* canvas, int w, int h) {
    const SkScalar x = SkIntToScalar(w) / 2;
    const SkScalar y = SkIntToScalar(h) / 2;

    canvas->translate(x, y);
    // just enough so we can't take the sprite case
    canvas->scale(SK_Scalar1 * 99/100, SK_Scalar1 * 99/100);
    canvas->translate(-x, -y);
}

static SkSurface* make_surface(SkColorType colorType, const SkIPoint& size,
                               Benchmark::Backend backend, int sampleCount,
                               GrContext* context) {
    SkSurface* surface = NULL;
    SkImageInfo info = SkImageInfo::Make(size.fX, size.fY, colorType,
                                         kPremul_SkAlphaType);

    switch (backend) {
        case Benchmark::kRaster_Backend:
            surface = SkSurface::NewRaster(info);
            surface->getCanvas()->clear(SK_ColorWHITE);
            break;
#if SK_SUPPORT_GPU
        case Benchmark::kGPU_Backend: {
            surface = SkSurface::NewRenderTarget(context, info, sampleCount);
            break;
        }
#endif
        case Benchmark::kPDF_Backend:
        default:
            SkDEBUGFAIL("unsupported");
    }
    return surface;
}

#if SK_SUPPORT_GPU
GrContextFactory gContextFactory;
typedef GrContextFactory::GLContextType GLContextType;
static const GLContextType kNative = GrContextFactory::kNative_GLContextType;
static const GLContextType kNVPR   = GrContextFactory::kNVPR_GLContextType;
#if SK_ANGLE
static const GLContextType kANGLE  = GrContextFactory::kANGLE_GLContextType;
#endif
static const GLContextType kDebug  = GrContextFactory::kDebug_GLContextType;
static const GLContextType kNull   = GrContextFactory::kNull_GLContextType;
#else
typedef int GLContextType;
static const GLContextType kNative = 0, kANGLE = 0, kDebug = 0, kNull = 0;
#endif

#ifdef SK_DEBUG
static const bool kIsDebug = true;
#else
static const bool kIsDebug = false;
#endif

static const struct Config {
    SkColorType         fColorType;
    const char*         name;
    int                 sampleCount;
    Benchmark::Backend  backend;
    GLContextType       contextType;
    bool                runByDefault;
} gConfigs[] = {
    { kN32_SkColorType,     "NONRENDERING", 0, Benchmark::kNonRendering_Backend, kNative, true},
    { kN32_SkColorType,     "8888",         0, Benchmark::kRaster_Backend,       kNative, true},
    { kRGB_565_SkColorType, "565",          0, Benchmark::kRaster_Backend,       kNative, true},
#if SK_SUPPORT_GPU
    { kN32_SkColorType,     "GPU",          0, Benchmark::kGPU_Backend,          kNative, true},
    { kN32_SkColorType,     "MSAA4",        4, Benchmark::kGPU_Backend,          kNative, false},
    { kN32_SkColorType,     "MSAA16",      16, Benchmark::kGPU_Backend,          kNative, false},
    { kN32_SkColorType,     "NVPRMSAA4",    4, Benchmark::kGPU_Backend,          kNVPR,   true},
    { kN32_SkColorType,     "NVPRMSAA16",  16, Benchmark::kGPU_Backend,          kNVPR,   false},
#if SK_ANGLE
    { kN32_SkColorType,     "ANGLE",        0, Benchmark::kGPU_Backend,          kANGLE,  true},
#endif // SK_ANGLE
    { kN32_SkColorType,     "Debug",        0, Benchmark::kGPU_Backend,          kDebug,  kIsDebug},
    { kN32_SkColorType,     "NULLGPU",      0, Benchmark::kGPU_Backend,          kNull,   true},
#endif // SK_SUPPORT_GPU
};

DEFINE_string(outDir, "", "If given, image of each bench will be put in outDir.");
DEFINE_string(timers, "cg", "Timers to display. "
              "Options: w(all) W(all, truncated) c(pu) C(pu, truncated) g(pu)");

DEFINE_bool(rotate, false,  "Rotate canvas before bench run?");
DEFINE_bool(scale,  false,  "Scale canvas before bench run?");
DEFINE_bool(clip,   false,  "Clip canvas before bench run?");

DEFINE_bool(forceAA,        true,     "Force anti-aliasing?");
DEFINE_bool(forceFilter,    false,    "Force bitmap filtering?");
DEFINE_string(forceDither, "default", "Force dithering: true, false, or default?");
DEFINE_bool(forceBlend,     false,    "Force alpha blending?");

DEFINE_int32(gpuCacheBytes, -1, "GPU cache size limit in bytes.  0 to disable cache.");
DEFINE_int32(gpuCacheCount, -1, "GPU cache size limit in object count.  0 to disable cache.");

DEFINE_bool2(leaks, l, false, "show leaked ref cnt'd objects.");
DEFINE_string(match, "",  "[~][^]substring[$] [...] of test name to run.\n"
                          "Multiple matches may be separated by spaces.\n"
                          "~ causes a matching test to always be skipped\n"
                          "^ requires the start of the test to match\n"
                          "$ requires the end of the test to match\n"
                          "^ and $ requires an exact match\n"
                          "If a test does not match any list entry,\n"
                          "it is skipped unless some list entry starts with ~\n");
DEFINE_string(mode, "normal",
             "normal:         draw to a normal canvas;\n"
             "deferred:       draw to a deferred canvas;\n"
             "deferredSilent: deferred with silent playback;\n"
             "record:         draw to an SkPicture;\n"
             "picturerecord:  draw from an SkPicture to an SkPicture.\n");
DEFINE_string(config, kDefaultsConfigStr,
              "Run configs given.  By default, runs the configs marked \"runByDefault\" in gConfigs.");
DEFINE_string(logFile, "", "Also write stdout here.");
DEFINE_int32(minMs, 20,  "Shortest time we'll allow a benchmark to run.");
DEFINE_int32(maxMs, 4000, "Longest time we'll allow a benchmark to run.");
DEFINE_bool(runOnce, kIsDebug, "Run each bench exactly once and don't report timings.");
DEFINE_double(error, 0.01,
              "Ratio of subsequent bench measurements must drop within 1±error to converge.");
DEFINE_string(timeFormat, "%9.2f", "Format to print results, in milliseconds per 1000 loops.");
DEFINE_bool2(verbose, v, false, "Print more.");
DEFINE_string(outResultsFile, "", "If given, the results will be written to the file in JSON format.");
DEFINE_bool(dryRun, false, "Don't actually run the tests, just print what would have been done.");

// Has this bench converged?  First arguments are milliseconds / loop iteration,
// last is overall runtime in milliseconds.
static bool HasConverged(double prevPerLoop, double currPerLoop, double currRaw) {
    if (currRaw < FLAGS_minMs) {
        return false;
    }
    const double low = 1 - FLAGS_error, high = 1 + FLAGS_error;
    const double ratio = currPerLoop / prevPerLoop;
    return low < ratio && ratio < high;
}

int tool_main(int argc, char** argv);
int tool_main(int argc, char** argv) {
    SetupCrashHandler();
    SkCommandLineFlags::Parse(argc, argv);
#if SK_ENABLE_INST_COUNT
    if (FLAGS_leaks) {
        gPrintInstCount = true;
    }
#endif
    SkAutoGraphics ag;

    // First, parse some flags.
    BenchLogger logger;
    if (FLAGS_logFile.count()) {
        logger.SetLogFile(FLAGS_logFile[0]);
    }

    LoggerResultsWriter logWriter(logger, FLAGS_timeFormat[0]);
    MultiResultsWriter writer;
    writer.add(&logWriter);

    SkAutoTDelete<JSONResultsWriter> jsonWriter;
    if (FLAGS_outResultsFile.count()) {
        jsonWriter.reset(SkNEW(JSONResultsWriter(FLAGS_outResultsFile[0])));
        writer.add(jsonWriter.get());
    }

    // Instantiate after all the writers have been added to writer so that we
    // call close() before their destructors are called on the way out.
    CallEnd<MultiResultsWriter> ender(writer);

    const uint8_t alpha = FLAGS_forceBlend ? 0x80 : 0xFF;
    SkTriState::State dither = SkTriState::kDefault;
    for (size_t i = 0; i < 3; i++) {
        if (strcmp(SkTriState::Name[i], FLAGS_forceDither[0]) == 0) {
            dither = static_cast<SkTriState::State>(i);
        }
    }

    BenchMode benchMode = kNormal_BenchMode;
    for (size_t i = 0; i < SK_ARRAY_COUNT(BenchMode_Name); i++) {
        if (strcmp(FLAGS_mode[0], BenchMode_Name[i]) == 0) {
            benchMode = static_cast<BenchMode>(i);
        }
    }

    SkTDArray<int> configs;
    bool runDefaultConfigs = false;
    // Try user-given configs first.
    for (int i = 0; i < FLAGS_config.count(); i++) {
        for (int j = 0; j < static_cast<int>(SK_ARRAY_COUNT(gConfigs)); ++j) {
            if (0 == strcmp(FLAGS_config[i], gConfigs[j].name)) {
                *configs.append() = j;
            } else if (0 == strcmp(FLAGS_config[i], kDefaultsConfigStr)) {
                runDefaultConfigs = true;
            }
        }
    }
    // If there weren't any, fill in with defaults.
    if (runDefaultConfigs) {
        for (int i = 0; i < static_cast<int>(SK_ARRAY_COUNT(gConfigs)); ++i) {
            if (gConfigs[i].runByDefault) {
                *configs.append() = i;
            }
        }
    }
    // Filter out things we can't run.
    if (kNormal_BenchMode != benchMode) {
        // Non-rendering configs only run in normal mode
        for (int i = 0; i < configs.count(); ++i) {
            const Config& config = gConfigs[configs[i]];
            if (Benchmark::kNonRendering_Backend == config.backend) {
                configs.remove(i, 1);
                --i;
            }
        }
    }

#if SK_SUPPORT_GPU
    for (int i = 0; i < configs.count(); ++i) {
        const Config& config = gConfigs[configs[i]];

        if (Benchmark::kGPU_Backend == config.backend) {
            GrContext* context = gContextFactory.get(config.contextType);
            if (NULL == context) {
                SkDebugf("GrContext could not be created for config %s. Config will be skipped.\n",
                    config.name);
                configs.remove(i);
                --i;
                continue;
            }
            if (config.sampleCount > context->getMaxSampleCount()){
                SkDebugf(
                    "Sample count (%d) for config %s is not supported. Config will be skipped.\n",
                    config.sampleCount, config.name);
                configs.remove(i);
                --i;
                continue;
            }
        }
    }
#endif

    // All flags should be parsed now.  Report our settings.
    if (FLAGS_runOnce) {
        logger.logError("bench was run with --runOnce, so we're going to hide the times."
                        " It's for your own good!\n");
    }
    writer.option("mode", FLAGS_mode[0]);
    writer.option("alpha", SkStringPrintf("0x%02X", alpha).c_str());
    writer.option("antialias", SkStringPrintf("%d", FLAGS_forceAA).c_str());
    writer.option("filter", SkStringPrintf("%d", FLAGS_forceFilter).c_str());
    writer.option("dither",  SkTriState::Name[dither]);

    writer.option("rotate", SkStringPrintf("%d", FLAGS_rotate).c_str());
    writer.option("scale", SkStringPrintf("%d", FLAGS_scale).c_str());
    writer.option("clip", SkStringPrintf("%d", FLAGS_clip).c_str());

#if defined(SK_BUILD_FOR_WIN32)
    writer.option("system", "WIN32");
#elif defined(SK_BUILD_FOR_MAC)
    writer.option("system", "MAC");
#elif defined(SK_BUILD_FOR_ANDROID)
    writer.option("system", "ANDROID");
#elif defined(SK_BUILD_FOR_UNIX)
    writer.option("system", "UNIX");
#else
    writer.option("system", "other");
#endif

#if defined(SK_DEBUG)
    writer.option("build", "DEBUG");
#else
    writer.option("build", "RELEASE");
#endif

    // Set texture cache limits if non-default.
    for (size_t i = 0; i < SK_ARRAY_COUNT(gConfigs); ++i) {
#if SK_SUPPORT_GPU
        const Config& config = gConfigs[i];
        if (Benchmark::kGPU_Backend != config.backend) {
            continue;
        }
        GrContext* context = gContextFactory.get(config.contextType);
        if (NULL == context) {
            continue;
        }

        size_t bytes;
        int count;
        context->getResourceCacheLimits(&count, &bytes);
        if (-1 != FLAGS_gpuCacheBytes) {
            bytes = static_cast<size_t>(FLAGS_gpuCacheBytes);
        }
        if (-1 != FLAGS_gpuCacheCount) {
            count = FLAGS_gpuCacheCount;
        }
        context->setResourceCacheLimits(count, bytes);
#endif
    }

    // Run each bench in each configuration it supports and we asked for.
    Iter iter;
    Benchmark* bench;
    while ((bench = iter.next()) != NULL) {
        SkAutoTUnref<Benchmark> benchUnref(bench);
        if (SkCommandLineFlags::ShouldSkip(FLAGS_match, bench->getName())) {
            continue;
        }

        bench->setForceAlpha(alpha);
        bench->setForceAA(FLAGS_forceAA);
        bench->setForceFilter(FLAGS_forceFilter);
        bench->setDither(dither);
        bench->preDraw();

        bool loggedBenchName = false;
        for (int i = 0; i < configs.count(); ++i) {
            const int configIndex = configs[i];
            const Config& config = gConfigs[configIndex];

            if (!bench->isSuitableFor(config.backend)) {
                continue;
            }

            GrContext* context = NULL;
#if SK_SUPPORT_GPU
            SkGLContextHelper* glContext = NULL;
            if (Benchmark::kGPU_Backend == config.backend) {
                context = gContextFactory.get(config.contextType);
                if (NULL == context) {
                    continue;
                }
                glContext = gContextFactory.getGLContext(config.contextType);
            }
#endif

            SkAutoTUnref<SkCanvas> canvas;
            SkAutoTUnref<SkPicture> recordFrom;
            SkPictureRecorder recorderTo;
            const SkIPoint dim = bench->getSize();

            SkAutoTUnref<SkSurface> surface;
            if (Benchmark::kNonRendering_Backend != config.backend) {
                surface.reset(make_surface(config.fColorType,
                                           dim,
                                           config.backend,
                                           config.sampleCount,
                                           context));
                if (!surface.get()) {
                    logger.logError(SkStringPrintf(
                        "Device creation failure for config %s. Will skip.\n", config.name));
                    continue;
                }

                switch(benchMode) {
                    case kDeferredSilent_BenchMode:
                    case kDeferred_BenchMode:
                        canvas.reset(SkDeferredCanvas::Create(surface.get()));
                        break;
                    case kRecord_BenchMode:
                        canvas.reset(SkRef(recorderTo.beginRecording(dim.fX, dim.fY)));
                        break;
                    case kPictureRecord_BenchMode: {
                        SkPictureRecorder recorderFrom;
                        bench->draw(1, recorderFrom.beginRecording(dim.fX, dim.fY));
                        recordFrom.reset(recorderFrom.endRecording());
                        canvas.reset(SkRef(recorderTo.beginRecording(dim.fX, dim.fY)));
                        break;
                    }
                    case kNormal_BenchMode:
                        canvas.reset(SkRef(surface->getCanvas()));
                        break;
                    default:
                        SkASSERT(false);
                }
            }

            if (NULL != canvas) {
                canvas->clear(SK_ColorWHITE);
                if (FLAGS_clip)   {
                    perform_clip(canvas, dim.fX, dim.fY);
                }
                if (FLAGS_scale)  {
                    perform_scale(canvas, dim.fX, dim.fY);
                }
                if (FLAGS_rotate) {
                    perform_rotate(canvas, dim.fX, dim.fY);
                }
            }

            if (!loggedBenchName) {
                loggedBenchName = true;
                writer.bench(bench->getName(), dim.fX, dim.fY);
            }

#if SK_SUPPORT_GPU
            SkGLContextHelper* contextHelper = NULL;
            if (Benchmark::kGPU_Backend == config.backend) {
                contextHelper = gContextFactory.getGLContext(config.contextType);
            }
            BenchTimer timer(contextHelper);
#else
            BenchTimer timer;
#endif

            double previous = std::numeric_limits<double>::infinity();
            bool converged = false;

            // variables used to compute loopsPerFrame
            double frameIntervalTime = 0.0f;
            int frameIntervalTotalLoops = 0;

            bool frameIntervalComputed = false;
            int loopsPerFrame = 0;
            int loopsPerIter = 0;
            if (FLAGS_verbose) { SkDebugf("%s %s: ", bench->getName(), config.name); }
            if (!FLAGS_dryRun) {
                do {
                    // Ramp up 1 -> 2 -> 4 -> 8 -> 16 -> ... -> ~1 billion.
                    loopsPerIter = (loopsPerIter == 0) ? 1 : loopsPerIter * 2;
                    if (loopsPerIter >= (1<<30) || timer.fWall > FLAGS_maxMs) {
                        // If you find it takes more than a billion loops to get up to 20ms of runtime,
                        // you've got a computer clocked at several THz or have a broken benchmark.  ;)
                        //     "1B ought to be enough for anybody."
                        logger.logError(SkStringPrintf(
                            "\nCan't get %s %s to converge in %dms (%d loops)",
                             bench->getName(), config.name, FLAGS_maxMs, loopsPerIter));
                        break;
                    }

                    if ((benchMode == kRecord_BenchMode || benchMode == kPictureRecord_BenchMode)) {
                        // Clear the recorded commands so that they do not accumulate.
                        canvas.reset(SkRef(recorderTo.beginRecording(dim.fX, dim.fY)));
                    }

                    timer.start();
                    // Inner loop that allows us to break the run into smaller
                    // chunks (e.g. frames). This is especially useful for the GPU
                    // as we can flush and/or swap buffers to keep the GPU from
                    // queuing up too much work.
                    for (int loopCount = loopsPerIter; loopCount > 0; ) {
                        // Save and restore around each call to draw() to guarantee a pristine canvas.
                        SkAutoCanvasRestore saveRestore(canvas, true/*also save*/);

                        int loops;
                        if (frameIntervalComputed && loopCount > loopsPerFrame) {
                            loops = loopsPerFrame;
                            loopCount -= loopsPerFrame;
                        } else {
                            loops = loopCount;
                            loopCount = 0;
                        }

                        if (benchMode == kPictureRecord_BenchMode) {
                            recordFrom->draw(canvas);
                        } else {
                            bench->draw(loops, canvas);
                        }

                        if (kDeferredSilent_BenchMode == benchMode) {
                            static_cast<SkDeferredCanvas*>(canvas.get())->silentFlush();
                        } else if (NULL != canvas) {
                            canvas->flush();
                        }

    #if SK_SUPPORT_GPU
                        // swap drawing buffers on each frame to prevent the GPU
                        // from queuing up too much work
                        if (NULL != glContext) {
                            glContext->swapBuffers();
                        }
    #endif
                    }



                    // Stop truncated timers before GL calls complete, and stop the full timers after.
                    timer.truncatedEnd();
    #if SK_SUPPORT_GPU
                    if (NULL != glContext) {
                        context->flush();
                        SK_GL(*glContext, Finish());
                    }
    #endif
                    timer.end();

                    // setup the frame interval for subsequent iterations
                    if (!frameIntervalComputed) {
                        frameIntervalTime += timer.fWall;
                        frameIntervalTotalLoops += loopsPerIter;
                        if (frameIntervalTime >= FLAGS_minMs) {
                            frameIntervalComputed = true;
                            loopsPerFrame =
                              (int)(((double)frameIntervalTotalLoops / frameIntervalTime) * FLAGS_minMs);
                            if (loopsPerFrame < 1) {
                                loopsPerFrame = 1;
                            }
    //                        SkDebugf("  %s has %d loops in %f ms (normalized to %d)\n",
    //                                 bench->getName(), frameIntervalTotalLoops,
    //                                 timer.fWall, loopsPerFrame);
                        }
                    }

                    const double current = timer.fWall / loopsPerIter;
                    if (FLAGS_verbose && current > previous) { SkDebugf("↑"); }
                    if (FLAGS_verbose) { SkDebugf("%.3g ", current); }
                    converged = HasConverged(previous, current, timer.fWall);
                    previous = current;
                } while (!FLAGS_runOnce && !converged);
            }
            if (FLAGS_verbose) { SkDebugf("\n"); }

            if (!FLAGS_dryRun && FLAGS_outDir.count() && Benchmark::kNonRendering_Backend != config.backend) {
                SkAutoTUnref<SkImage> image(surface->newImageSnapshot());
                if (image.get()) {
                    saveFile(bench->getName(), config.name, FLAGS_outDir[0],
                             image);
                }
            }

            if (FLAGS_runOnce) {
                // Let's not mislead ourselves by looking at Debug build or single iteration bench times!
                continue;
            }

            // Normalize to ms per 1000 iterations.
            const double normalize = 1000.0 / loopsPerIter;
            const struct { char shortName; const char* longName; double ms; } times[] = {
                {'w', "msecs",  normalize * timer.fWall},
                {'W', "Wmsecs", normalize * timer.fTruncatedWall},
                {'c', "cmsecs", normalize * timer.fCpu},
                {'C', "Cmsecs", normalize * timer.fTruncatedCpu},
                {'g', "gmsecs", normalize * timer.fGpu},
            };

            writer.config(config.name);
            for (size_t i = 0; i < SK_ARRAY_COUNT(times); i++) {
                if (strchr(FLAGS_timers[0], times[i].shortName) && times[i].ms > 0) {
                    writer.timer(times[i].longName, times[i].ms);
                }
            }
        }
    }
#if SK_SUPPORT_GPU
    gContextFactory.destroyContexts();
#endif
    return 0;
}

#if !defined(SK_BUILD_FOR_IOS) && !defined(SK_BUILD_FOR_NACL)
int main(int argc, char * const argv[]) {
    return tool_main(argc, (char**) argv);
}
#endif
