/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "DMPDFTask.h"
#include "DMPDFRasterizeTask.h"
#include "DMUtil.h"
#include "DMWriteTask.h"
#include "SkCommandLineFlags.h"
#include "SkDocument.h"

// The PDF backend is not threadsafe.  If you run dm with --pdf repeatedly, you
// will quickly find yourself crashed.  (while catchsegv out/Release/dm;; end).
//
// TODO(mtklein): re-enable by default, maybe moving to its own single thread.
DEFINE_bool(pdf, false, "PDF backend master switch.");

namespace DM {

PDFTask::PDFTask(const char* config,
                 Reporter* reporter,
                 TaskRunner* taskRunner,
                 skiagm::GMRegistry::Factory factory,
                 RasterizePdfProc rasterizePdfProc)
    : CpuTask(reporter, taskRunner)
    , fGM(factory(NULL))
    , fName(UnderJoin(fGM->getName(), config))
    , fRasterize(rasterizePdfProc) {}

PDFTask::PDFTask(Reporter* reporter,
                 TaskRunner* taskRunner,
                 SkPicture* picture,
                 SkString filename,
                 RasterizePdfProc rasterizePdfProc)
    : CpuTask(reporter, taskRunner)
    , fPicture(SkRef(picture))
    , fName(UnderJoin(FileToTaskName(filename).c_str(), "pdf"))
    , fRasterize(rasterizePdfProc) {}

namespace {

class SinglePagePDF {
public:
    SinglePagePDF(SkScalar width, SkScalar height)
        : fDocument(SkDocument::CreatePDF(&fWriteStream))
        , fCanvas(fDocument->beginPage(width, height)) {}

    SkCanvas* canvas() { return fCanvas; }

    SkData* end() {
        fCanvas->flush();
        fDocument->endPage();
        fDocument->close();
        return fWriteStream.copyToData();
    }

private:
    SkDynamicMemoryWStream fWriteStream;
    SkAutoTUnref<SkDocument> fDocument;
    SkCanvas* fCanvas;
};

}  // namespace

void PDFTask::draw() {
    SkAutoTUnref<SkData> pdfData;
    bool rasterize = true;
    if (fGM.get()) {
        rasterize = 0 == (fGM->getFlags() & skiagm::GM::kSkipPDFRasterization_Flag);
        SinglePagePDF pdf(fGM->width(), fGM->height());
        //TODO(mtklein): GM doesn't do this.  Why not?
        //pdf.canvas()->concat(fGM->getInitialTransform());
        fGM->draw(pdf.canvas());
        pdfData.reset(pdf.end());
    } else {
        SinglePagePDF pdf(SkIntToScalar(fPicture->width()), SkIntToScalar(fPicture->height()));
        fPicture->draw(pdf.canvas());
        pdfData.reset(pdf.end());
    }

    SkASSERT(pdfData.get());
    if (rasterize) {
        this->spawnChild(SkNEW_ARGS(PDFRasterizeTask, (*this, pdfData.get(), fRasterize)));
    }
    this->spawnChild(SkNEW_ARGS(WriteTask, (*this, pdfData.get(), ".pdf")));
}

bool PDFTask::shouldSkip() const {
    if (!FLAGS_pdf) {
        return true;
    }
    if (fGM.get() && 0 != (fGM->getFlags() & skiagm::GM::kSkipPDF_Flag)) {
        return true;
    }
    return false;
}

}  // namespace DM
