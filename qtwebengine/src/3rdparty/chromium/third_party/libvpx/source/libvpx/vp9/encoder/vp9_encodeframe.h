/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_ENCODEFRAME_H_
#define VP9_ENCODER_VP9_ENCODEFRAME_H_

#ifdef __cplusplus
extern "C" {
#endif

struct macroblock;
struct yv12_buffer_config;
struct VP9_COMP;

typedef struct {
  unsigned int sse;
  int sum;
  unsigned int var;
} diff;

void vp9_setup_src_planes(struct macroblock *x,
                          const struct yv12_buffer_config *src,
                          int mi_row, int mi_col);

void vp9_encode_frame(struct VP9_COMP *cpi);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP9_ENCODER_VP9_ENCODEFRAME_H_
