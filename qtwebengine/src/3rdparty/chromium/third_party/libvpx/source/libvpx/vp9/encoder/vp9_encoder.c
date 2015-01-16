/*
 * Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <limits.h>

#include "./vpx_config.h"
#include "./vpx_scale_rtcd.h"
#include "vpx/internal/vpx_psnr.h"
#include "vpx_ports/vpx_timer.h"

#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_filter.h"
#include "vp9/common/vp9_idct.h"
#if CONFIG_VP9_POSTPROC
#include "vp9/common/vp9_postproc.h"
#endif
#include "vp9/common/vp9_reconinter.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vp9/common/vp9_tile_common.h"

#include "vp9/encoder/vp9_aq_complexity.h"
#include "vp9/encoder/vp9_aq_cyclicrefresh.h"
#include "vp9/encoder/vp9_aq_variance.h"
#include "vp9/encoder/vp9_bitstream.h"
#include "vp9/encoder/vp9_context_tree.h"
#include "vp9/encoder/vp9_encodeframe.h"
#include "vp9/encoder/vp9_encodemv.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/encoder/vp9_mbgraph.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_picklpf.h"
#include "vp9/encoder/vp9_ratectrl.h"
#include "vp9/encoder/vp9_rdopt.h"
#include "vp9/encoder/vp9_segmentation.h"
#include "vp9/encoder/vp9_speed_features.h"
#if CONFIG_INTERNAL_STATS
#include "vp9/encoder/vp9_ssim.h"
#endif
#include "vp9/encoder/vp9_temporal_filter.h"
#include "vp9/encoder/vp9_resize.h"
#include "vp9/encoder/vp9_svc_layercontext.h"

void vp9_coef_tree_initialize();

#define DEFAULT_INTERP_FILTER SWITCHABLE

#define SHARP_FILTER_QTHRESH 0          /* Q threshold for 8-tap sharp filter */

#define ALTREF_HIGH_PRECISION_MV 1      // Whether to use high precision mv
                                         //  for altref computation.
#define HIGH_PRECISION_MV_QTHRESH 200   // Q threshold for high precision
                                         // mv. Choose a very high value for
                                         // now so that HIGH_PRECISION is always
                                         // chosen.

// #define OUTPUT_YUV_REC

#ifdef OUTPUT_YUV_SRC
FILE *yuv_file;
#endif
#ifdef OUTPUT_YUV_REC
FILE *yuv_rec_file;
#endif

#if 0
FILE *framepsnr;
FILE *kf_list;
FILE *keyfile;
#endif

static INLINE void Scale2Ratio(VPX_SCALING mode, int *hr, int *hs) {
  switch (mode) {
    case NORMAL:
      *hr = 1;
      *hs = 1;
      break;
    case FOURFIVE:
      *hr = 4;
      *hs = 5;
      break;
    case THREEFIVE:
      *hr = 3;
      *hs = 5;
    break;
    case ONETWO:
      *hr = 1;
      *hs = 2;
    break;
    default:
      *hr = 1;
      *hs = 1;
       assert(0);
      break;
  }
}

static void set_high_precision_mv(VP9_COMP *cpi, int allow_high_precision_mv) {
  MACROBLOCK *const mb = &cpi->mb;
  cpi->common.allow_high_precision_mv = allow_high_precision_mv;
  if (cpi->common.allow_high_precision_mv) {
    mb->mvcost = mb->nmvcost_hp;
    mb->mvsadcost = mb->nmvsadcost_hp;
  } else {
    mb->mvcost = mb->nmvcost;
    mb->mvsadcost = mb->nmvsadcost;
  }
}

static void setup_frame(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  // Set up entropy context depending on frame type. The decoder mandates
  // the use of the default context, index 0, for keyframes and inter
  // frames where the error_resilient_mode or intra_only flag is set. For
  // other inter-frames the encoder currently uses only two contexts;
  // context 1 for ALTREF frames and context 0 for the others.
  if (frame_is_intra_only(cm) || cm->error_resilient_mode) {
    vp9_setup_past_independence(cm);
  } else {
    if (!cpi->use_svc)
      cm->frame_context_idx = cpi->refresh_alt_ref_frame;
  }

  if (cm->frame_type == KEY_FRAME) {
    cpi->refresh_golden_frame = 1;
    cpi->refresh_alt_ref_frame = 1;
  } else {
    cm->fc = cm->frame_contexts[cm->frame_context_idx];
  }
}

void vp9_initialize_enc() {
  static int init_done = 0;

  if (!init_done) {
    vp9_init_neighbors();
    vp9_init_quant_tables();

    vp9_coef_tree_initialize();
    vp9_tokenize_initialize();
    vp9_init_me_luts();
    vp9_rc_init_minq_luts();
    vp9_entropy_mv_init();
    vp9_entropy_mode_init();
    vp9_temporal_filter_init();
    init_done = 1;
  }
}

static void dealloc_compressor_data(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  int i;

  // Delete sementation map
  vpx_free(cpi->segmentation_map);
  cpi->segmentation_map = NULL;
  vpx_free(cm->last_frame_seg_map);
  cm->last_frame_seg_map = NULL;
  vpx_free(cpi->coding_context.last_frame_seg_map_copy);
  cpi->coding_context.last_frame_seg_map_copy = NULL;

  vpx_free(cpi->complexity_map);
  cpi->complexity_map = NULL;

  vp9_cyclic_refresh_free(cpi->cyclic_refresh);
  cpi->cyclic_refresh = NULL;

  vpx_free(cpi->active_map);
  cpi->active_map = NULL;

  vp9_free_frame_buffers(cm);

  vp9_free_frame_buffer(&cpi->last_frame_uf);
  vp9_free_frame_buffer(&cpi->scaled_source);
  vp9_free_frame_buffer(&cpi->scaled_last_source);
  vp9_free_frame_buffer(&cpi->alt_ref_buffer);
  vp9_lookahead_destroy(cpi->lookahead);

  vpx_free(cpi->tok);
  cpi->tok = 0;

  vp9_free_pc_tree(&cpi->mb);

  for (i = 0; i < cpi->svc.number_spatial_layers; ++i) {
    LAYER_CONTEXT *const lc = &cpi->svc.layer_context[i];
    vpx_free(lc->rc_twopass_stats_in.buf);
    lc->rc_twopass_stats_in.buf = NULL;
    lc->rc_twopass_stats_in.sz = 0;
  }
}

static void save_coding_context(VP9_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  VP9_COMMON *cm = &cpi->common;

  // Stores a snapshot of key state variables which can subsequently be
  // restored with a call to vp9_restore_coding_context. These functions are
  // intended for use in a re-code loop in vp9_compress_frame where the
  // quantizer value is adjusted between loop iterations.
  vp9_copy(cc->nmvjointcost,  cpi->mb.nmvjointcost);
  vp9_copy(cc->nmvcosts,  cpi->mb.nmvcosts);
  vp9_copy(cc->nmvcosts_hp,  cpi->mb.nmvcosts_hp);

  vp9_copy(cc->segment_pred_probs, cm->seg.pred_probs);

  vpx_memcpy(cpi->coding_context.last_frame_seg_map_copy,
             cm->last_frame_seg_map, (cm->mi_rows * cm->mi_cols));

  vp9_copy(cc->last_ref_lf_deltas, cm->lf.last_ref_deltas);
  vp9_copy(cc->last_mode_lf_deltas, cm->lf.last_mode_deltas);

  cc->fc = cm->fc;
}

static void restore_coding_context(VP9_COMP *cpi) {
  CODING_CONTEXT *const cc = &cpi->coding_context;
  VP9_COMMON *cm = &cpi->common;

  // Restore key state variables to the snapshot state stored in the
  // previous call to vp9_save_coding_context.
  vp9_copy(cpi->mb.nmvjointcost, cc->nmvjointcost);
  vp9_copy(cpi->mb.nmvcosts, cc->nmvcosts);
  vp9_copy(cpi->mb.nmvcosts_hp, cc->nmvcosts_hp);

  vp9_copy(cm->seg.pred_probs, cc->segment_pred_probs);

  vpx_memcpy(cm->last_frame_seg_map,
             cpi->coding_context.last_frame_seg_map_copy,
             (cm->mi_rows * cm->mi_cols));

  vp9_copy(cm->lf.last_ref_deltas, cc->last_ref_lf_deltas);
  vp9_copy(cm->lf.last_mode_deltas, cc->last_mode_lf_deltas);

  cm->fc = cc->fc;
}

static void configure_static_seg_features(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  struct segmentation *const seg = &cm->seg;

  int high_q = (int)(rc->avg_q > 48.0);
  int qi_delta;

  // Disable and clear down for KF
  if (cm->frame_type == KEY_FRAME) {
    // Clear down the global segmentation map
    vpx_memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    seg->update_map = 0;
    seg->update_data = 0;
    cpi->static_mb_pct = 0;

    // Disable segmentation
    vp9_disable_segmentation(seg);

    // Clear down the segment features.
    vp9_clearall_segfeatures(seg);
  } else if (cpi->refresh_alt_ref_frame) {
    // If this is an alt ref frame
    // Clear down the global segmentation map
    vpx_memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    seg->update_map = 0;
    seg->update_data = 0;
    cpi->static_mb_pct = 0;

    // Disable segmentation and individual segment features by default
    vp9_disable_segmentation(seg);
    vp9_clearall_segfeatures(seg);

    // Scan frames from current to arf frame.
    // This function re-enables segmentation if appropriate.
    vp9_update_mbgraph_stats(cpi);

    // If segmentation was enabled set those features needed for the
    // arf itself.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;

      qi_delta = vp9_compute_qdelta(rc, rc->avg_q, rc->avg_q * 0.875);
      vp9_set_segdata(seg, 1, SEG_LVL_ALT_Q, qi_delta - 2);
      vp9_set_segdata(seg, 1, SEG_LVL_ALT_LF, -2);

      vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);
      vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_LF);

      // Where relevant assume segment data is delta data
      seg->abs_delta = SEGMENT_DELTADATA;
    }
  } else if (seg->enabled) {
    // All other frames if segmentation has been enabled

    // First normal frame in a valid gf or alt ref group
    if (rc->frames_since_golden == 0) {
      // Set up segment features for normal frames in an arf group
      if (rc->source_alt_ref_active) {
        seg->update_map = 0;
        seg->update_data = 1;
        seg->abs_delta = SEGMENT_DELTADATA;

        qi_delta = vp9_compute_qdelta(rc, rc->avg_q, rc->avg_q * 1.125);
        vp9_set_segdata(seg, 1, SEG_LVL_ALT_Q, qi_delta + 2);
        vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_Q);

        vp9_set_segdata(seg, 1, SEG_LVL_ALT_LF, -2);
        vp9_enable_segfeature(seg, 1, SEG_LVL_ALT_LF);

        // Segment coding disabled for compred testing
        if (high_q || (cpi->static_mb_pct == 100)) {
          vp9_set_segdata(seg, 1, SEG_LVL_REF_FRAME, ALTREF_FRAME);
          vp9_enable_segfeature(seg, 1, SEG_LVL_REF_FRAME);
          vp9_enable_segfeature(seg, 1, SEG_LVL_SKIP);
        }
      } else {
        // Disable segmentation and clear down features if alt ref
        // is not active for this group

        vp9_disable_segmentation(seg);

        vpx_memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);

        seg->update_map = 0;
        seg->update_data = 0;

        vp9_clearall_segfeatures(seg);
      }
    } else if (rc->is_src_frame_alt_ref) {
      // Special case where we are coding over the top of a previous
      // alt ref frame.
      // Segment coding disabled for compred testing

      // Enable ref frame features for segment 0 as well
      vp9_enable_segfeature(seg, 0, SEG_LVL_REF_FRAME);
      vp9_enable_segfeature(seg, 1, SEG_LVL_REF_FRAME);

      // All mbs should use ALTREF_FRAME
      vp9_clear_segdata(seg, 0, SEG_LVL_REF_FRAME);
      vp9_set_segdata(seg, 0, SEG_LVL_REF_FRAME, ALTREF_FRAME);
      vp9_clear_segdata(seg, 1, SEG_LVL_REF_FRAME);
      vp9_set_segdata(seg, 1, SEG_LVL_REF_FRAME, ALTREF_FRAME);

      // Skip all MBs if high Q (0,0 mv and skip coeffs)
      if (high_q) {
        vp9_enable_segfeature(seg, 0, SEG_LVL_SKIP);
        vp9_enable_segfeature(seg, 1, SEG_LVL_SKIP);
      }
      // Enable data update
      seg->update_data = 1;
    } else {
      // All other frames.

      // No updates.. leave things as they are.
      seg->update_map = 0;
      seg->update_data = 0;
    }
  }
}

static void update_reference_segmentation_map(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  MODE_INFO **mi_8x8_ptr = cm->mi_grid_visible;
  uint8_t *cache_ptr = cm->last_frame_seg_map;
  int row, col;

  for (row = 0; row < cm->mi_rows; row++) {
    MODE_INFO **mi_8x8 = mi_8x8_ptr;
    uint8_t *cache = cache_ptr;
    for (col = 0; col < cm->mi_cols; col++, mi_8x8++, cache++)
      cache[0] = mi_8x8[0]->mbmi.segment_id;
    mi_8x8_ptr += cm->mi_stride;
    cache_ptr += cm->mi_cols;
  }
}


static void set_speed_features(VP9_COMP *cpi) {
#if CONFIG_INTERNAL_STATS
  int i;
  for (i = 0; i < MAX_MODES; ++i)
    cpi->mode_chosen_counts[i] = 0;
#endif

  vp9_set_speed_features(cpi);

  // Set rd thresholds based on mode and speed setting
  vp9_set_rd_speed_thresholds(cpi);
  vp9_set_rd_speed_thresholds_sub8x8(cpi);

  cpi->mb.fwd_txm4x4 = vp9_fdct4x4;
  if (cpi->oxcf.lossless || cpi->mb.e_mbd.lossless) {
    cpi->mb.fwd_txm4x4 = vp9_fwht4x4;
  }
}

static void alloc_raw_frame_buffers(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  const VP9EncoderConfig *oxcf = &cpi->oxcf;

  cpi->lookahead = vp9_lookahead_init(oxcf->width, oxcf->height,
                                      cm->subsampling_x, cm->subsampling_y,
                                      oxcf->lag_in_frames);
  if (!cpi->lookahead)
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate lag buffers");

  if (vp9_realloc_frame_buffer(&cpi->alt_ref_buffer,
                               oxcf->width, oxcf->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate altref buffer");
}

void vp9_alloc_compressor_data(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;

  if (vp9_alloc_frame_buffers(cm, cm->width, cm->height))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate frame buffers");

  if (vp9_alloc_frame_buffer(&cpi->last_frame_uf,
                             cm->width, cm->height,
                             cm->subsampling_x, cm->subsampling_y,
                             VP9_ENC_BORDER_IN_PIXELS))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate last frame buffer");

  if (vp9_alloc_frame_buffer(&cpi->scaled_source,
                             cm->width, cm->height,
                             cm->subsampling_x, cm->subsampling_y,
                             VP9_ENC_BORDER_IN_PIXELS))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate scaled source buffer");

  if (vp9_alloc_frame_buffer(&cpi->scaled_last_source,
                             cm->width, cm->height,
                             cm->subsampling_x, cm->subsampling_y,
                             VP9_ENC_BORDER_IN_PIXELS))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to allocate scaled last source buffer");

  vpx_free(cpi->tok);

  {
    unsigned int tokens = get_token_alloc(cm->mb_rows, cm->mb_cols);

    CHECK_MEM_ERROR(cm, cpi->tok, vpx_calloc(tokens, sizeof(*cpi->tok)));
  }

  vp9_setup_pc_tree(&cpi->common, &cpi->mb);
}

static void update_frame_size(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;

  vp9_update_frame_size(cm);

  // Update size of buffers local to this frame
  if (vp9_realloc_frame_buffer(&cpi->last_frame_uf,
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to reallocate last frame buffer");

  if (vp9_realloc_frame_buffer(&cpi->scaled_source,
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to reallocate scaled source buffer");

  if (vp9_realloc_frame_buffer(&cpi->scaled_last_source,
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL))
    vpx_internal_error(&cm->error, VPX_CODEC_MEM_ERROR,
                       "Failed to reallocate scaled last source buffer");

  {
    int y_stride = cpi->scaled_source.y_stride;

    if (cpi->sf.search_method == NSTEP) {
      vp9_init3smotion_compensation(&cpi->ss_cfg, y_stride);
    } else if (cpi->sf.search_method == DIAMOND) {
      vp9_init_dsmotion_compensation(&cpi->ss_cfg, y_stride);
    }
  }

  init_macroblockd(cm, xd);
}

void vp9_new_framerate(VP9_COMP *cpi, double framerate) {
  cpi->oxcf.framerate = framerate < 0.1 ? 30 : framerate;
  vp9_rc_update_framerate(cpi);
}

int64_t vp9_rescale(int64_t val, int64_t num, int denom) {
  int64_t llnum = num;
  int64_t llden = denom;
  int64_t llval = val;

  return (llval * llnum / llden);
}

static void set_tile_limits(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;

  int min_log2_tile_cols, max_log2_tile_cols;
  vp9_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  cm->log2_tile_cols = clamp(cpi->oxcf.tile_columns,
                             min_log2_tile_cols, max_log2_tile_cols);
  cm->log2_tile_rows = cpi->oxcf.tile_rows;
}

static void init_config(struct VP9_COMP *cpi, VP9EncoderConfig *oxcf) {
  VP9_COMMON *const cm = &cpi->common;

  cpi->oxcf = *oxcf;

  cm->profile = oxcf->profile;
  cm->bit_depth = oxcf->bit_depth;

  cm->width = oxcf->width;
  cm->height = oxcf->height;
  cm->subsampling_x = 0;
  cm->subsampling_y = 0;
  vp9_alloc_compressor_data(cpi);

  // Spatial scalability.
  cpi->svc.number_spatial_layers = oxcf->ss_number_layers;
  // Temporal scalability.
  cpi->svc.number_temporal_layers = oxcf->ts_number_layers;

  if ((cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.rc_mode == RC_MODE_CBR) ||
      (cpi->svc.number_spatial_layers > 1 &&
      cpi->oxcf.mode == TWO_PASS_SECOND_BEST)) {
    vp9_init_layer_context(cpi);
  }

  // change includes all joint functionality
  vp9_change_config(cpi, oxcf);

  cpi->static_mb_pct = 0;

  cpi->lst_fb_idx = 0;
  cpi->gld_fb_idx = 1;
  cpi->alt_fb_idx = 2;

  set_tile_limits(cpi);
}

static int get_pass(MODE mode) {
  switch (mode) {
    case REALTIME:
    case ONE_PASS_GOOD:
    case ONE_PASS_BEST:
      return 0;

    case TWO_PASS_FIRST:
      return 1;

    case TWO_PASS_SECOND_GOOD:
    case TWO_PASS_SECOND_BEST:
      return 2;
  }
  return -1;
}

void vp9_change_config(struct VP9_COMP *cpi, const VP9EncoderConfig *oxcf) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;

  if (cm->profile != oxcf->profile)
    cm->profile = oxcf->profile;
  cm->bit_depth = oxcf->bit_depth;

  if (cm->profile <= PROFILE_1)
    assert(cm->bit_depth == BITS_8);
  else
    assert(cm->bit_depth > BITS_8);

  cpi->oxcf = *oxcf;
  cpi->pass = get_pass(cpi->oxcf.mode);
  if (cpi->oxcf.mode == REALTIME)
    cpi->oxcf.play_alternate = 0;

  cpi->oxcf.lossless = oxcf->lossless;
  if (cpi->oxcf.lossless) {
    // In lossless mode, make sure right quantizer range and correct transform
    // is set.
    cpi->oxcf.worst_allowed_q = 0;
    cpi->oxcf.best_allowed_q = 0;
    cpi->mb.e_mbd.itxm_add = vp9_iwht4x4_add;
  } else {
    cpi->mb.e_mbd.itxm_add = vp9_idct4x4_add;
  }
  rc->baseline_gf_interval = DEFAULT_GF_INTERVAL;
  cpi->ref_frame_flags = VP9_ALT_FLAG | VP9_GOLD_FLAG | VP9_LAST_FLAG;

  cpi->refresh_golden_frame = 0;
  cpi->refresh_last_frame = 1;
  cm->refresh_frame_context = 1;
  cm->reset_frame_context = 0;

  vp9_reset_segment_features(&cm->seg);
  set_high_precision_mv(cpi, 0);

  {
    int i;

    for (i = 0; i < MAX_SEGMENTS; i++)
      cpi->segment_encode_breakout[i] = cpi->oxcf.encode_breakout;
  }
  cpi->encode_breakout = cpi->oxcf.encode_breakout;

  // local file playback mode == really big buffer
  if (cpi->oxcf.rc_mode == RC_MODE_VBR) {
    cpi->oxcf.starting_buffer_level   = 60000;
    cpi->oxcf.optimal_buffer_level    = 60000;
    cpi->oxcf.maximum_buffer_size     = 240000;
  }

  // Convert target bandwidth from Kbit/s to Bit/s
  cpi->oxcf.target_bandwidth       *= 1000;

  cpi->oxcf.starting_buffer_level =
      vp9_rescale(cpi->oxcf.starting_buffer_level,
                  cpi->oxcf.target_bandwidth, 1000);

  // Set or reset optimal and maximum buffer levels.
  if (cpi->oxcf.optimal_buffer_level == 0)
    cpi->oxcf.optimal_buffer_level = cpi->oxcf.target_bandwidth / 8;
  else
    cpi->oxcf.optimal_buffer_level =
        vp9_rescale(cpi->oxcf.optimal_buffer_level,
                    cpi->oxcf.target_bandwidth, 1000);

  if (cpi->oxcf.maximum_buffer_size == 0)
    cpi->oxcf.maximum_buffer_size = cpi->oxcf.target_bandwidth / 8;
  else
    cpi->oxcf.maximum_buffer_size =
        vp9_rescale(cpi->oxcf.maximum_buffer_size,
                    cpi->oxcf.target_bandwidth, 1000);
  // Under a configuration change, where maximum_buffer_size may change,
  // keep buffer level clipped to the maximum allowed buffer size.
  rc->bits_off_target = MIN(rc->bits_off_target, cpi->oxcf.maximum_buffer_size);
  rc->buffer_level = MIN(rc->buffer_level, cpi->oxcf.maximum_buffer_size);

  // Set up frame rate and related parameters rate control values.
  vp9_new_framerate(cpi, cpi->oxcf.framerate);

  // Set absolute upper and lower quality limits
  rc->worst_quality = cpi->oxcf.worst_allowed_q;
  rc->best_quality = cpi->oxcf.best_allowed_q;

  cm->interp_filter = DEFAULT_INTERP_FILTER;

  cm->display_width = cpi->oxcf.width;
  cm->display_height = cpi->oxcf.height;

  if (cpi->initial_width) {
    // Increasing the size of the frame beyond the first seen frame, or some
    // otherwise signaled maximum size, is not supported.
    // TODO(jkoleszar): exit gracefully.
    assert(cm->width <= cpi->initial_width);
    assert(cm->height <= cpi->initial_height);
  }
  update_frame_size(cpi);

  if ((cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.rc_mode == RC_MODE_CBR) ||
      (cpi->svc.number_spatial_layers > 1 && cpi->pass == 2)) {
    vp9_update_layer_context_change_config(cpi,
                                           (int)cpi->oxcf.target_bandwidth);
  }

#if CONFIG_MULTIPLE_ARF
  vp9_zero(cpi->alt_ref_source);
#else
  cpi->alt_ref_source = NULL;
#endif
  rc->is_src_frame_alt_ref = 0;

#if 0
  // Experimental RD Code
  cpi->frame_distortion = 0;
  cpi->last_frame_distortion = 0;
#endif

  set_tile_limits(cpi);

  cpi->ext_refresh_frame_flags_pending = 0;
  cpi->ext_refresh_frame_context_pending = 0;
}

#ifndef M_LOG2_E
#define M_LOG2_E 0.693147180559945309417
#endif
#define log2f(x) (log (x) / (float) M_LOG2_E)

static void cal_nmvjointsadcost(int *mvjointsadcost) {
  mvjointsadcost[0] = 600;
  mvjointsadcost[1] = 300;
  mvjointsadcost[2] = 300;
  mvjointsadcost[3] = 300;
}

static void cal_nmvsadcosts(int *mvsadcost[2]) {
  int i = 1;

  mvsadcost[0][0] = 0;
  mvsadcost[1][0] = 0;

  do {
    double z = 256 * (2 * (log2f(8 * i) + .6));
    mvsadcost[0][i] = (int)z;
    mvsadcost[1][i] = (int)z;
    mvsadcost[0][-i] = (int)z;
    mvsadcost[1][-i] = (int)z;
  } while (++i <= MV_MAX);
}

static void cal_nmvsadcosts_hp(int *mvsadcost[2]) {
  int i = 1;

  mvsadcost[0][0] = 0;
  mvsadcost[1][0] = 0;

  do {
    double z = 256 * (2 * (log2f(8 * i) + .6));
    mvsadcost[0][i] = (int)z;
    mvsadcost[1][i] = (int)z;
    mvsadcost[0][-i] = (int)z;
    mvsadcost[1][-i] = (int)z;
  } while (++i <= MV_MAX);
}


VP9_COMP *vp9_create_compressor(VP9EncoderConfig *oxcf) {
  unsigned int i, j;
  VP9_COMP *const cpi = vpx_memalign(32, sizeof(VP9_COMP));
  VP9_COMMON *const cm = cpi != NULL ? &cpi->common : NULL;

  if (!cm)
    return NULL;

  vp9_zero(*cpi);

  if (setjmp(cm->error.jmp)) {
    cm->error.setjmp = 0;
    vp9_remove_compressor(cpi);
    return 0;
  }

  cm->error.setjmp = 1;

  vp9_rtcd();

  cpi->use_svc = 0;

  init_config(cpi, oxcf);
  vp9_rc_init(&cpi->oxcf, cpi->pass, &cpi->rc);

  cm->current_video_frame = 0;

  // Set reference frame sign bias for ALTREF frame to 1 (for now)
  cm->ref_frame_sign_bias[ALTREF_FRAME] = 1;

  cpi->gold_is_last = 0;
  cpi->alt_is_last = 0;
  cpi->gold_is_alt = 0;

  // Create the encoder segmentation map and set all entries to 0
  CHECK_MEM_ERROR(cm, cpi->segmentation_map,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));

  // Create a complexity map used for rd adjustment
  CHECK_MEM_ERROR(cm, cpi->complexity_map,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));

  // Create a map used for cyclic background refresh.
  CHECK_MEM_ERROR(cm, cpi->cyclic_refresh,
                  vp9_cyclic_refresh_alloc(cm->mi_rows, cm->mi_cols));

  // And a place holder structure is the coding context
  // for use if we want to save and restore it
  CHECK_MEM_ERROR(cm, cpi->coding_context.last_frame_seg_map_copy,
                  vpx_calloc(cm->mi_rows * cm->mi_cols, 1));

  CHECK_MEM_ERROR(cm, cpi->active_map, vpx_calloc(cm->MBs, 1));
  vpx_memset(cpi->active_map, 1, cm->MBs);
  cpi->active_map_enabled = 0;

  for (i = 0; i < (sizeof(cpi->mbgraph_stats) /
                   sizeof(cpi->mbgraph_stats[0])); i++) {
    CHECK_MEM_ERROR(cm, cpi->mbgraph_stats[i].mb_stats,
                    vpx_calloc(cm->MBs *
                               sizeof(*cpi->mbgraph_stats[i].mb_stats), 1));
  }

  cpi->refresh_alt_ref_frame = 0;

#if CONFIG_MULTIPLE_ARF
  // Turn multiple ARF usage on/off. This is a quick hack for the initial test
  // version. It should eventually be set via the codec API.
  cpi->multi_arf_enabled = 1;

  if (cpi->multi_arf_enabled) {
    cpi->sequence_number = 0;
    cpi->frame_coding_order_period = 0;
    vp9_zero(cpi->frame_coding_order);
    vp9_zero(cpi->arf_buffer_idx);
  }
#endif

  cpi->b_calculate_psnr = CONFIG_INTERNAL_STATS;
#if CONFIG_INTERNAL_STATS
  cpi->b_calculate_ssimg = 0;

  cpi->count = 0;
  cpi->bytes = 0;

  if (cpi->b_calculate_psnr) {
    cpi->total_y = 0.0;
    cpi->total_u = 0.0;
    cpi->total_v = 0.0;
    cpi->total = 0.0;
    cpi->total_sq_error = 0;
    cpi->total_samples = 0;

    cpi->totalp_y = 0.0;
    cpi->totalp_u = 0.0;
    cpi->totalp_v = 0.0;
    cpi->totalp = 0.0;
    cpi->totalp_sq_error = 0;
    cpi->totalp_samples = 0;

    cpi->tot_recode_hits = 0;
    cpi->summed_quality = 0;
    cpi->summed_weights = 0;
    cpi->summedp_quality = 0;
    cpi->summedp_weights = 0;
  }

  if (cpi->b_calculate_ssimg) {
    cpi->total_ssimg_y = 0;
    cpi->total_ssimg_u = 0;
    cpi->total_ssimg_v = 0;
    cpi->total_ssimg_all = 0;
  }

#endif

  cpi->first_time_stamp_ever = INT64_MAX;

  cal_nmvjointsadcost(cpi->mb.nmvjointsadcost);
  cpi->mb.nmvcost[0] = &cpi->mb.nmvcosts[0][MV_MAX];
  cpi->mb.nmvcost[1] = &cpi->mb.nmvcosts[1][MV_MAX];
  cpi->mb.nmvsadcost[0] = &cpi->mb.nmvsadcosts[0][MV_MAX];
  cpi->mb.nmvsadcost[1] = &cpi->mb.nmvsadcosts[1][MV_MAX];
  cal_nmvsadcosts(cpi->mb.nmvsadcost);

  cpi->mb.nmvcost_hp[0] = &cpi->mb.nmvcosts_hp[0][MV_MAX];
  cpi->mb.nmvcost_hp[1] = &cpi->mb.nmvcosts_hp[1][MV_MAX];
  cpi->mb.nmvsadcost_hp[0] = &cpi->mb.nmvsadcosts_hp[0][MV_MAX];
  cpi->mb.nmvsadcost_hp[1] = &cpi->mb.nmvsadcosts_hp[1][MV_MAX];
  cal_nmvsadcosts_hp(cpi->mb.nmvsadcost_hp);

#ifdef OUTPUT_YUV_SRC
  yuv_file = fopen("bd.yuv", "ab");
#endif
#ifdef OUTPUT_YUV_REC
  yuv_rec_file = fopen("rec.yuv", "wb");
#endif

#if 0
  framepsnr = fopen("framepsnr.stt", "a");
  kf_list = fopen("kf_list.stt", "w");
#endif

  cpi->output_pkt_list = oxcf->output_pkt_list;

  cpi->allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;

  if (cpi->pass == 1) {
    vp9_init_first_pass(cpi);
  } else if (cpi->pass == 2) {
    const size_t packet_sz = sizeof(FIRSTPASS_STATS);
    const int packets = (int)(oxcf->two_pass_stats_in.sz / packet_sz);

    if (cpi->svc.number_spatial_layers > 1
        && cpi->svc.number_temporal_layers == 1) {
      FIRSTPASS_STATS *const stats = oxcf->two_pass_stats_in.buf;
      FIRSTPASS_STATS *stats_copy[VPX_SS_MAX_LAYERS] = {0};
      int i;

      for (i = 0; i < oxcf->ss_number_layers; ++i) {
        FIRSTPASS_STATS *const last_packet_for_layer =
            &stats[packets - oxcf->ss_number_layers + i];
        const int layer_id = (int)last_packet_for_layer->spatial_layer_id;
        const int packets_in_layer = (int)last_packet_for_layer->count + 1;
        if (layer_id >= 0 && layer_id < oxcf->ss_number_layers) {
          LAYER_CONTEXT *const lc = &cpi->svc.layer_context[layer_id];

          vpx_free(lc->rc_twopass_stats_in.buf);

          lc->rc_twopass_stats_in.sz = packets_in_layer * packet_sz;
          CHECK_MEM_ERROR(cm, lc->rc_twopass_stats_in.buf,
                          vpx_malloc(lc->rc_twopass_stats_in.sz));
          lc->twopass.stats_in_start = lc->rc_twopass_stats_in.buf;
          lc->twopass.stats_in = lc->twopass.stats_in_start;
          lc->twopass.stats_in_end = lc->twopass.stats_in_start
                                     + packets_in_layer - 1;
          stats_copy[layer_id] = lc->rc_twopass_stats_in.buf;
        }
      }

      for (i = 0; i < packets; ++i) {
        const int layer_id = (int)stats[i].spatial_layer_id;
        if (layer_id >= 0 && layer_id < oxcf->ss_number_layers
            && stats_copy[layer_id] != NULL) {
          *stats_copy[layer_id] = stats[i];
          ++stats_copy[layer_id];
        }
      }

      vp9_init_second_pass_spatial_svc(cpi);
    } else {
      cpi->twopass.stats_in_start = oxcf->two_pass_stats_in.buf;
      cpi->twopass.stats_in = cpi->twopass.stats_in_start;
      cpi->twopass.stats_in_end = &cpi->twopass.stats_in[packets - 1];

      vp9_init_second_pass(cpi);
    }
  }

  set_speed_features(cpi);

  // Default rd threshold factors for mode selection
  for (i = 0; i < BLOCK_SIZES; ++i) {
    for (j = 0; j < MAX_MODES; ++j)
      cpi->rd.thresh_freq_fact[i][j] = 32;
  }

#define BFP(BT, SDF, SDAF, VF, SVF, SVAF, SDX3F, SDX8F, SDX4DF)\
    cpi->fn_ptr[BT].sdf            = SDF; \
    cpi->fn_ptr[BT].sdaf           = SDAF; \
    cpi->fn_ptr[BT].vf             = VF; \
    cpi->fn_ptr[BT].svf            = SVF; \
    cpi->fn_ptr[BT].svaf           = SVAF; \
    cpi->fn_ptr[BT].sdx3f          = SDX3F; \
    cpi->fn_ptr[BT].sdx8f          = SDX8F; \
    cpi->fn_ptr[BT].sdx4df         = SDX4DF;

  BFP(BLOCK_32X16, vp9_sad32x16, vp9_sad32x16_avg,
      vp9_variance32x16, vp9_sub_pixel_variance32x16,
      vp9_sub_pixel_avg_variance32x16, NULL, NULL, vp9_sad32x16x4d)

  BFP(BLOCK_16X32, vp9_sad16x32, vp9_sad16x32_avg,
      vp9_variance16x32, vp9_sub_pixel_variance16x32,
      vp9_sub_pixel_avg_variance16x32, NULL, NULL, vp9_sad16x32x4d)

  BFP(BLOCK_64X32, vp9_sad64x32, vp9_sad64x32_avg,
      vp9_variance64x32, vp9_sub_pixel_variance64x32,
      vp9_sub_pixel_avg_variance64x32, NULL, NULL, vp9_sad64x32x4d)

  BFP(BLOCK_32X64, vp9_sad32x64, vp9_sad32x64_avg,
      vp9_variance32x64, vp9_sub_pixel_variance32x64,
      vp9_sub_pixel_avg_variance32x64, NULL, NULL, vp9_sad32x64x4d)

  BFP(BLOCK_32X32, vp9_sad32x32, vp9_sad32x32_avg,
      vp9_variance32x32, vp9_sub_pixel_variance32x32,
      vp9_sub_pixel_avg_variance32x32, vp9_sad32x32x3, vp9_sad32x32x8,
      vp9_sad32x32x4d)

  BFP(BLOCK_64X64, vp9_sad64x64, vp9_sad64x64_avg,
      vp9_variance64x64, vp9_sub_pixel_variance64x64,
      vp9_sub_pixel_avg_variance64x64, vp9_sad64x64x3, vp9_sad64x64x8,
      vp9_sad64x64x4d)

  BFP(BLOCK_16X16, vp9_sad16x16, vp9_sad16x16_avg,
      vp9_variance16x16, vp9_sub_pixel_variance16x16,
      vp9_sub_pixel_avg_variance16x16, vp9_sad16x16x3, vp9_sad16x16x8,
      vp9_sad16x16x4d)

  BFP(BLOCK_16X8, vp9_sad16x8, vp9_sad16x8_avg,
      vp9_variance16x8, vp9_sub_pixel_variance16x8,
      vp9_sub_pixel_avg_variance16x8,
      vp9_sad16x8x3, vp9_sad16x8x8, vp9_sad16x8x4d)

  BFP(BLOCK_8X16, vp9_sad8x16, vp9_sad8x16_avg,
      vp9_variance8x16, vp9_sub_pixel_variance8x16,
      vp9_sub_pixel_avg_variance8x16,
      vp9_sad8x16x3, vp9_sad8x16x8, vp9_sad8x16x4d)

  BFP(BLOCK_8X8, vp9_sad8x8, vp9_sad8x8_avg,
      vp9_variance8x8, vp9_sub_pixel_variance8x8,
      vp9_sub_pixel_avg_variance8x8,
      vp9_sad8x8x3, vp9_sad8x8x8, vp9_sad8x8x4d)

  BFP(BLOCK_8X4, vp9_sad8x4, vp9_sad8x4_avg,
      vp9_variance8x4, vp9_sub_pixel_variance8x4,
      vp9_sub_pixel_avg_variance8x4, NULL, vp9_sad8x4x8, vp9_sad8x4x4d)

  BFP(BLOCK_4X8, vp9_sad4x8, vp9_sad4x8_avg,
      vp9_variance4x8, vp9_sub_pixel_variance4x8,
      vp9_sub_pixel_avg_variance4x8, NULL, vp9_sad4x8x8, vp9_sad4x8x4d)

  BFP(BLOCK_4X4, vp9_sad4x4, vp9_sad4x4_avg,
      vp9_variance4x4, vp9_sub_pixel_variance4x4,
      vp9_sub_pixel_avg_variance4x4,
      vp9_sad4x4x3, vp9_sad4x4x8, vp9_sad4x4x4d)

  cpi->full_search_sad = vp9_full_search_sad;
  cpi->diamond_search_sad = vp9_diamond_search_sad;
  cpi->refining_search_sad = vp9_refining_search_sad;

  /* vp9_init_quantizer() is first called here. Add check in
   * vp9_frame_init_quantizer() so that vp9_init_quantizer is only
   * called later when needed. This will avoid unnecessary calls of
   * vp9_init_quantizer() for every frame.
   */
  vp9_init_quantizer(cpi);

  vp9_loop_filter_init(cm);

  cm->error.setjmp = 0;

  return cpi;
}

void vp9_remove_compressor(VP9_COMP *cpi) {
  unsigned int i;

  if (!cpi)
    return;

  if (cpi && (cpi->common.current_video_frame > 0)) {
#if CONFIG_INTERNAL_STATS

    vp9_clear_system_state();

    // printf("\n8x8-4x4:%d-%d\n", cpi->t8x8_count, cpi->t4x4_count);
    if (cpi->pass != 1) {
      FILE *f = fopen("opsnr.stt", "a");
      double time_encoded = (cpi->last_end_time_stamp_seen
                             - cpi->first_time_stamp_ever) / 10000000.000;
      double total_encode_time = (cpi->time_receive_data +
                                  cpi->time_compress_data)   / 1000.000;
      double dr = (double)cpi->bytes * (double) 8 / (double)1000
                  / time_encoded;

      if (cpi->b_calculate_psnr) {
        const double total_psnr =
            vpx_sse_to_psnr((double)cpi->total_samples, 255.0,
                            (double)cpi->total_sq_error);
        const double totalp_psnr =
            vpx_sse_to_psnr((double)cpi->totalp_samples, 255.0,
                            (double)cpi->totalp_sq_error);
        const double total_ssim = 100 * pow(cpi->summed_quality /
                                                cpi->summed_weights, 8.0);
        const double totalp_ssim = 100 * pow(cpi->summedp_quality /
                                                cpi->summedp_weights, 8.0);

        fprintf(f, "Bitrate\tAVGPsnr\tGLBPsnr\tAVPsnrP\tGLPsnrP\t"
                "VPXSSIM\tVPSSIMP\t  Time(ms)\n");
        fprintf(f, "%7.2f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%7.3f\t%8.0f\n",
                dr, cpi->total / cpi->count, total_psnr,
                cpi->totalp / cpi->count, totalp_psnr, total_ssim, totalp_ssim,
                total_encode_time);
      }

      if (cpi->b_calculate_ssimg) {
        fprintf(f, "BitRate\tSSIM_Y\tSSIM_U\tSSIM_V\tSSIM_A\t  Time(ms)\n");
        fprintf(f, "%7.2f\t%6.4f\t%6.4f\t%6.4f\t%6.4f\t%8.0f\n", dr,
                cpi->total_ssimg_y / cpi->count,
                cpi->total_ssimg_u / cpi->count,
                cpi->total_ssimg_v / cpi->count,
                cpi->total_ssimg_all / cpi->count, total_encode_time);
      }

      fclose(f);
    }

#endif

#if 0
    {
      printf("\n_pick_loop_filter_level:%d\n", cpi->time_pick_lpf / 1000);
      printf("\n_frames recive_data encod_mb_row compress_frame  Total\n");
      printf("%6d %10ld %10ld %10ld %10ld\n", cpi->common.current_video_frame,
             cpi->time_receive_data / 1000, cpi->time_encode_sb_row / 1000,
             cpi->time_compress_data / 1000,
             (cpi->time_receive_data + cpi->time_compress_data) / 1000);
    }
#endif
  }

  dealloc_compressor_data(cpi);
  vpx_free(cpi->tok);

  for (i = 0; i < sizeof(cpi->mbgraph_stats) /
                  sizeof(cpi->mbgraph_stats[0]); ++i) {
    vpx_free(cpi->mbgraph_stats[i].mb_stats);
  }

  vp9_remove_common(&cpi->common);
  vpx_free(cpi);

#ifdef OUTPUT_YUV_SRC
  fclose(yuv_file);
#endif
#ifdef OUTPUT_YUV_REC
  fclose(yuv_rec_file);
#endif

#if 0

  if (keyfile)
    fclose(keyfile);

  if (framepsnr)
    fclose(framepsnr);

  if (kf_list)
    fclose(kf_list);

#endif
}
static int64_t get_sse(const uint8_t *a, int a_stride,
                       const uint8_t *b, int b_stride,
                       int width, int height) {
  const int dw = width % 16;
  const int dh = height % 16;
  int64_t total_sse = 0;
  unsigned int sse = 0;
  int sum = 0;
  int x, y;

  if (dw > 0) {
    variance(&a[width - dw], a_stride, &b[width - dw], b_stride,
             dw, height, &sse, &sum);
    total_sse += sse;
  }

  if (dh > 0) {
    variance(&a[(height - dh) * a_stride], a_stride,
             &b[(height - dh) * b_stride], b_stride,
             width - dw, dh, &sse, &sum);
    total_sse += sse;
  }

  for (y = 0; y < height / 16; ++y) {
    const uint8_t *pa = a;
    const uint8_t *pb = b;
    for (x = 0; x < width / 16; ++x) {
      vp9_mse16x16(pa, a_stride, pb, b_stride, &sse);
      total_sse += sse;

      pa += 16;
      pb += 16;
    }

    a += 16 * a_stride;
    b += 16 * b_stride;
  }

  return total_sse;
}

typedef struct {
  double psnr[4];       // total/y/u/v
  uint64_t sse[4];      // total/y/u/v
  uint32_t samples[4];  // total/y/u/v
} PSNR_STATS;

static void calc_psnr(const YV12_BUFFER_CONFIG *a, const YV12_BUFFER_CONFIG *b,
                      PSNR_STATS *psnr) {
  const int widths[3]        = {a->y_width,  a->uv_width,  a->uv_width };
  const int heights[3]       = {a->y_height, a->uv_height, a->uv_height};
  const uint8_t *a_planes[3] = {a->y_buffer, a->u_buffer,  a->v_buffer };
  const int a_strides[3]     = {a->y_stride, a->uv_stride, a->uv_stride};
  const uint8_t *b_planes[3] = {b->y_buffer, b->u_buffer,  b->v_buffer };
  const int b_strides[3]     = {b->y_stride, b->uv_stride, b->uv_stride};
  int i;
  uint64_t total_sse = 0;
  uint32_t total_samples = 0;

  for (i = 0; i < 3; ++i) {
    const int w = widths[i];
    const int h = heights[i];
    const uint32_t samples = w * h;
    const uint64_t sse = get_sse(a_planes[i], a_strides[i],
                                 b_planes[i], b_strides[i],
                                 w, h);
    psnr->sse[1 + i] = sse;
    psnr->samples[1 + i] = samples;
    psnr->psnr[1 + i] = vpx_sse_to_psnr(samples, 255.0, (double)sse);

    total_sse += sse;
    total_samples += samples;
  }

  psnr->sse[0] = total_sse;
  psnr->samples[0] = total_samples;
  psnr->psnr[0] = vpx_sse_to_psnr((double)total_samples, 255.0,
                                  (double)total_sse);
}

static void generate_psnr_packet(VP9_COMP *cpi) {
  struct vpx_codec_cx_pkt pkt;
  int i;
  PSNR_STATS psnr;
  calc_psnr(cpi->Source, cpi->common.frame_to_show, &psnr);
  for (i = 0; i < 4; ++i) {
    pkt.data.psnr.samples[i] = psnr.samples[i];
    pkt.data.psnr.sse[i] = psnr.sse[i];
    pkt.data.psnr.psnr[i] = psnr.psnr[i];
  }
  pkt.kind = VPX_CODEC_PSNR_PKT;
  vpx_codec_pkt_list_add(cpi->output_pkt_list, &pkt);
}

int vp9_use_as_reference(VP9_COMP *cpi, int ref_frame_flags) {
  if (ref_frame_flags > 7)
    return -1;

  cpi->ref_frame_flags = ref_frame_flags;
  return 0;
}

void vp9_update_reference(VP9_COMP *cpi, int ref_frame_flags) {
  cpi->ext_refresh_golden_frame = (ref_frame_flags & VP9_GOLD_FLAG) != 0;
  cpi->ext_refresh_alt_ref_frame = (ref_frame_flags & VP9_ALT_FLAG) != 0;
  cpi->ext_refresh_last_frame = (ref_frame_flags & VP9_LAST_FLAG) != 0;
  cpi->ext_refresh_frame_flags_pending = 1;
}

static YV12_BUFFER_CONFIG *get_vp9_ref_frame_buffer(VP9_COMP *cpi,
                                VP9_REFFRAME ref_frame_flag) {
  MV_REFERENCE_FRAME ref_frame = NONE;
  if (ref_frame_flag == VP9_LAST_FLAG)
    ref_frame = LAST_FRAME;
  else if (ref_frame_flag == VP9_GOLD_FLAG)
    ref_frame = GOLDEN_FRAME;
  else if (ref_frame_flag == VP9_ALT_FLAG)
    ref_frame = ALTREF_FRAME;

  return ref_frame == NONE ? NULL : get_ref_frame_buffer(cpi, ref_frame);
}

int vp9_copy_reference_enc(VP9_COMP *cpi, VP9_REFFRAME ref_frame_flag,
                           YV12_BUFFER_CONFIG *sd) {
  YV12_BUFFER_CONFIG *cfg = get_vp9_ref_frame_buffer(cpi, ref_frame_flag);
  if (cfg) {
    vp8_yv12_copy_frame(cfg, sd);
    return 0;
  } else {
    return -1;
  }
}

int vp9_get_reference_enc(VP9_COMP *cpi, int index, YV12_BUFFER_CONFIG **fb) {
  VP9_COMMON *cm = &cpi->common;

  if (index < 0 || index >= REF_FRAMES)
    return -1;

  *fb = &cm->frame_bufs[cm->ref_frame_map[index]].buf;
  return 0;
}

int vp9_set_reference_enc(VP9_COMP *cpi, VP9_REFFRAME ref_frame_flag,
                          YV12_BUFFER_CONFIG *sd) {
  YV12_BUFFER_CONFIG *cfg = get_vp9_ref_frame_buffer(cpi, ref_frame_flag);
  if (cfg) {
    vp8_yv12_copy_frame(sd, cfg);
    return 0;
  } else {
    return -1;
  }
}

int vp9_update_entropy(VP9_COMP * cpi, int update) {
  cpi->ext_refresh_frame_context = update;
  cpi->ext_refresh_frame_context_pending = 1;
  return 0;
}


#ifdef OUTPUT_YUV_SRC
void vp9_write_yuv_frame(YV12_BUFFER_CONFIG *s) {
  uint8_t *src = s->y_buffer;
  int h = s->y_height;

  do {
    fwrite(src, s->y_width, 1,  yuv_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1,  yuv_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_file);
    src += s->uv_stride;
  } while (--h);
}
#endif

#ifdef OUTPUT_YUV_REC
void vp9_write_yuv_rec_frame(VP9_COMMON *cm) {
  YV12_BUFFER_CONFIG *s = cm->frame_to_show;
  uint8_t *src = s->y_buffer;
  int h = cm->height;

  do {
    fwrite(src, s->y_width, 1,  yuv_rec_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1,  yuv_rec_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = s->uv_height;

  do {
    fwrite(src, s->uv_width, 1, yuv_rec_file);
    src += s->uv_stride;
  } while (--h);

#if CONFIG_ALPHA
  if (s->alpha_buffer) {
    src = s->alpha_buffer;
    h = s->alpha_height;
    do {
      fwrite(src, s->alpha_width, 1,  yuv_rec_file);
      src += s->alpha_stride;
    } while (--h);
  }
#endif

  fflush(yuv_rec_file);
}
#endif

static void scale_and_extend_frame_nonnormative(const YV12_BUFFER_CONFIG *src,
                                                YV12_BUFFER_CONFIG *dst) {
  // TODO(dkovalev): replace YV12_BUFFER_CONFIG with vpx_image_t
  int i;
  const uint8_t *const srcs[4] = {src->y_buffer, src->u_buffer, src->v_buffer,
                                  src->alpha_buffer};
  const int src_strides[4] = {src->y_stride, src->uv_stride, src->uv_stride,
                              src->alpha_stride};
  const int src_widths[4] = {src->y_crop_width, src->uv_crop_width,
                             src->uv_crop_width, src->y_crop_width};
  const int src_heights[4] = {src->y_crop_height, src->uv_crop_height,
                              src->uv_crop_height, src->y_crop_height};
  uint8_t *const dsts[4] = {dst->y_buffer, dst->u_buffer, dst->v_buffer,
                            dst->alpha_buffer};
  const int dst_strides[4] = {dst->y_stride, dst->uv_stride, dst->uv_stride,
                              dst->alpha_stride};
  const int dst_widths[4] = {dst->y_crop_width, dst->uv_crop_width,
                             dst->uv_crop_width, dst->y_crop_width};
  const int dst_heights[4] = {dst->y_crop_height, dst->uv_crop_height,
                              dst->uv_crop_height, dst->y_crop_height};

  for (i = 0; i < MAX_MB_PLANE; ++i)
    vp9_resize_plane(srcs[i], src_heights[i], src_widths[i], src_strides[i],
                     dsts[i], dst_heights[i], dst_widths[i], dst_strides[i]);

  // TODO(hkuang): Call C version explicitly
  // as neon version only expand border size 32.
  vp8_yv12_extend_frame_borders_c(dst);
}

static void scale_and_extend_frame(const YV12_BUFFER_CONFIG *src,
                                   YV12_BUFFER_CONFIG *dst) {
  const int src_w = src->y_crop_width;
  const int src_h = src->y_crop_height;
  const int dst_w = dst->y_crop_width;
  const int dst_h = dst->y_crop_height;
  const uint8_t *const srcs[4] = {src->y_buffer, src->u_buffer, src->v_buffer,
                                  src->alpha_buffer};
  const int src_strides[4] = {src->y_stride, src->uv_stride, src->uv_stride,
                              src->alpha_stride};
  uint8_t *const dsts[4] = {dst->y_buffer, dst->u_buffer, dst->v_buffer,
                            dst->alpha_buffer};
  const int dst_strides[4] = {dst->y_stride, dst->uv_stride, dst->uv_stride,
                              dst->alpha_stride};
  int x, y, i;

  for (y = 0; y < dst_h; y += 16) {
    for (x = 0; x < dst_w; x += 16) {
      for (i = 0; i < MAX_MB_PLANE; ++i) {
        const int factor = (i == 0 || i == 3 ? 1 : 2);
        const int x_q4 = x * (16 / factor) * src_w / dst_w;
        const int y_q4 = y * (16 / factor) * src_h / dst_h;
        const int src_stride = src_strides[i];
        const int dst_stride = dst_strides[i];
        const uint8_t *src_ptr = srcs[i] + (y / factor) * src_h / dst_h *
                                     src_stride + (x / factor) * src_w / dst_w;
        uint8_t *dst_ptr = dsts[i] + (y / factor) * dst_stride + (x / factor);

        vp9_convolve8(src_ptr, src_stride, dst_ptr, dst_stride,
                      vp9_sub_pel_filters_8[x_q4 & 0xf], 16 * src_w / dst_w,
                      vp9_sub_pel_filters_8[y_q4 & 0xf], 16 * src_h / dst_h,
                      16 / factor, 16 / factor);
      }
    }
  }

  // TODO(hkuang): Call C version explicitly
  // as neon version only expand border size 32.
  vp8_yv12_extend_frame_borders_c(dst);
}

static int find_fp_qindex() {
  int i;

  for (i = 0; i < QINDEX_RANGE; i++) {
    if (vp9_convert_qindex_to_q(i) >= 30.0) {
      break;
    }
  }

  if (i == QINDEX_RANGE)
    i--;

  return i;
}

#define WRITE_RECON_BUFFER 0
#if WRITE_RECON_BUFFER
void write_cx_frame_to_file(YV12_BUFFER_CONFIG *frame, int this_frame) {
  FILE *yframe;
  int i;
  char filename[255];

  snprintf(filename, sizeof(filename), "cx\\y%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->y_height; i++)
    fwrite(frame->y_buffer + i * frame->y_stride,
           frame->y_width, 1, yframe);

  fclose(yframe);
  snprintf(filename, sizeof(filename), "cx\\u%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->uv_height; i++)
    fwrite(frame->u_buffer + i * frame->uv_stride,
           frame->uv_width, 1, yframe);

  fclose(yframe);
  snprintf(filename, sizeof(filename), "cx\\v%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->uv_height; i++)
    fwrite(frame->v_buffer + i * frame->uv_stride,
           frame->uv_width, 1, yframe);

  fclose(yframe);
}
#endif

// Function to test for conditions that indicate we should loop
// back and recode a frame.
static int recode_loop_test(const VP9_COMP *cpi,
                            int high_limit, int low_limit,
                            int q, int maxq, int minq) {
  const VP9_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  const VP9EncoderConfig *const oxcf = &cpi->oxcf;
  int force_recode = 0;

  // Special case trap if maximum allowed frame size exceeded.
  if (rc->projected_frame_size > rc->max_frame_bandwidth) {
    force_recode = 1;

  // Is frame recode allowed.
  // Yes if either recode mode 1 is selected or mode 2 is selected
  // and the frame is a key frame, golden frame or alt_ref_frame
  } else if ((cpi->sf.recode_loop == ALLOW_RECODE) ||
             ((cpi->sf.recode_loop == ALLOW_RECODE_KFARFGF) &&
              (cm->frame_type == KEY_FRAME ||
               cpi->refresh_golden_frame || cpi->refresh_alt_ref_frame))) {
    // General over and under shoot tests
    if ((rc->projected_frame_size > high_limit && q < maxq) ||
        (rc->projected_frame_size < low_limit && q > minq)) {
      force_recode = 1;
    } else if (cpi->oxcf.rc_mode == RC_MODE_CONSTRAINED_QUALITY) {
      // Deal with frame undershoot and whether or not we are
      // below the automatically set cq level.
      if (q > oxcf->cq_level &&
          rc->projected_frame_size < ((rc->this_frame_target * 7) >> 3)) {
        force_recode = 1;
      }
    }
  }
  return force_recode;
}

void vp9_update_reference_frames(VP9_COMP *cpi) {
  VP9_COMMON * const cm = &cpi->common;

  // At this point the new frame has been encoded.
  // If any buffer copy / swapping is signaled it should be done here.
  if (cm->frame_type == KEY_FRAME) {
    ref_cnt_fb(cm->frame_bufs,
               &cm->ref_frame_map[cpi->gld_fb_idx], cm->new_fb_idx);
    ref_cnt_fb(cm->frame_bufs,
               &cm->ref_frame_map[cpi->alt_fb_idx], cm->new_fb_idx);
  }
#if CONFIG_MULTIPLE_ARF
  else if (!cpi->multi_arf_enabled && cpi->refresh_golden_frame &&
      !cpi->refresh_alt_ref_frame) {
#else
  else if (cpi->refresh_golden_frame && !cpi->refresh_alt_ref_frame &&
           !cpi->use_svc) {
#endif
    /* Preserve the previously existing golden frame and update the frame in
     * the alt ref slot instead. This is highly specific to the current use of
     * alt-ref as a forward reference, and this needs to be generalized as
     * other uses are implemented (like RTC/temporal scaling)
     *
     * The update to the buffer in the alt ref slot was signaled in
     * vp9_pack_bitstream(), now swap the buffer pointers so that it's treated
     * as the golden frame next time.
     */
    int tmp;

    ref_cnt_fb(cm->frame_bufs,
               &cm->ref_frame_map[cpi->alt_fb_idx], cm->new_fb_idx);

    tmp = cpi->alt_fb_idx;
    cpi->alt_fb_idx = cpi->gld_fb_idx;
    cpi->gld_fb_idx = tmp;
  }  else { /* For non key/golden frames */
    if (cpi->refresh_alt_ref_frame) {
      int arf_idx = cpi->alt_fb_idx;
#if CONFIG_MULTIPLE_ARF
      if (cpi->multi_arf_enabled) {
        arf_idx = cpi->arf_buffer_idx[cpi->sequence_number + 1];
      }
#endif
      ref_cnt_fb(cm->frame_bufs,
                 &cm->ref_frame_map[arf_idx], cm->new_fb_idx);
    }

    if (cpi->refresh_golden_frame) {
      ref_cnt_fb(cm->frame_bufs,
                 &cm->ref_frame_map[cpi->gld_fb_idx], cm->new_fb_idx);
    }
  }

  if (cpi->refresh_last_frame) {
    ref_cnt_fb(cm->frame_bufs,
               &cm->ref_frame_map[cpi->lst_fb_idx], cm->new_fb_idx);
  }
}

static void loopfilter_frame(VP9_COMP *cpi, VP9_COMMON *cm) {
  MACROBLOCKD *xd = &cpi->mb.e_mbd;
  struct loopfilter *lf = &cm->lf;
  if (xd->lossless) {
      lf->filter_level = 0;
  } else {
    struct vpx_usec_timer timer;

    vp9_clear_system_state();

    vpx_usec_timer_start(&timer);

    vp9_pick_filter_level(cpi->Source, cpi, cpi->sf.lpf_pick);

    vpx_usec_timer_mark(&timer);
    cpi->time_pick_lpf += vpx_usec_timer_elapsed(&timer);
  }

  if (lf->filter_level > 0) {
    vp9_loop_filter_frame(cm->frame_to_show, cm, xd, lf->filter_level, 0, 0);
  }

  vp9_extend_frame_inner_borders(cm->frame_to_show);
}

void vp9_scale_references(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  MV_REFERENCE_FRAME ref_frame;

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const int idx = cm->ref_frame_map[get_ref_frame_idx(cpi, ref_frame)];
    const YV12_BUFFER_CONFIG *const ref = &cm->frame_bufs[idx].buf;

    if (ref->y_crop_width != cm->width ||
        ref->y_crop_height != cm->height) {
      const int new_fb = get_free_fb(cm);
      vp9_realloc_frame_buffer(&cm->frame_bufs[new_fb].buf,
                               cm->width, cm->height,
                               cm->subsampling_x, cm->subsampling_y,
                               VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL);
      scale_and_extend_frame(ref, &cm->frame_bufs[new_fb].buf);
      cpi->scaled_ref_idx[ref_frame - 1] = new_fb;
    } else {
      cpi->scaled_ref_idx[ref_frame - 1] = idx;
      cm->frame_bufs[idx].ref_count++;
    }
  }
}

static void release_scaled_references(VP9_COMP *cpi) {
  VP9_COMMON *cm = &cpi->common;
  int i;

  for (i = 0; i < 3; i++)
    cm->frame_bufs[cpi->scaled_ref_idx[i]].ref_count--;
}

static void full_to_model_count(unsigned int *model_count,
                                unsigned int *full_count) {
  int n;
  model_count[ZERO_TOKEN] = full_count[ZERO_TOKEN];
  model_count[ONE_TOKEN] = full_count[ONE_TOKEN];
  model_count[TWO_TOKEN] = full_count[TWO_TOKEN];
  for (n = THREE_TOKEN; n < EOB_TOKEN; ++n)
    model_count[TWO_TOKEN] += full_count[n];
  model_count[EOB_MODEL_TOKEN] = full_count[EOB_TOKEN];
}

static void full_to_model_counts(vp9_coeff_count_model *model_count,
                                 vp9_coeff_count *full_count) {
  int i, j, k, l;

  for (i = 0; i < PLANE_TYPES; ++i)
    for (j = 0; j < REF_TYPES; ++j)
      for (k = 0; k < COEF_BANDS; ++k)
        for (l = 0; l < BAND_COEFF_CONTEXTS(k); ++l)
          full_to_model_count(model_count[i][j][k][l], full_count[i][j][k][l]);
}

#if 0 && CONFIG_INTERNAL_STATS
static void output_frame_level_debug_stats(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  FILE *const f = fopen("tmp.stt", cm->current_video_frame ? "a" : "w");
  int recon_err;

  vp9_clear_system_state();

  recon_err = vp9_get_y_sse(cpi->Source, get_frame_new_buffer(cm));

  if (cpi->twopass.total_left_stats.coded_error != 0.0)
    fprintf(f, "%10u %10d %10d %10d %10d"
        "%10"PRId64" %10"PRId64" %10"PRId64" %10"PRId64" %10d "
        "%7.2lf %7.2lf %7.2lf %7.2lf %7.2lf"
        "%6d %6d %5d %5d %5d "
        "%10"PRId64" %10.3lf"
        "%10lf %8u %10d %10d %10d\n",
        cpi->common.current_video_frame, cpi->rc.this_frame_target,
        cpi->rc.projected_frame_size,
        cpi->rc.projected_frame_size / cpi->common.MBs,
        (cpi->rc.projected_frame_size - cpi->rc.this_frame_target),
        cpi->rc.vbr_bits_off_target,
        cpi->rc.total_target_vs_actual,
        (cpi->oxcf.starting_buffer_level - cpi->rc.bits_off_target),
        cpi->rc.total_actual_bits, cm->base_qindex,
        vp9_convert_qindex_to_q(cm->base_qindex),
        (double)vp9_dc_quant(cm->base_qindex, 0) / 4.0,
        cpi->rc.avg_q,
        vp9_convert_qindex_to_q(cpi->rc.ni_av_qi),
        vp9_convert_qindex_to_q(cpi->oxcf.cq_level),
        cpi->refresh_last_frame, cpi->refresh_golden_frame,
        cpi->refresh_alt_ref_frame, cm->frame_type, cpi->rc.gfu_boost,
        cpi->twopass.bits_left,
        cpi->twopass.total_left_stats.coded_error,
        cpi->twopass.bits_left /
            (1 + cpi->twopass.total_left_stats.coded_error),
        cpi->tot_recode_hits, recon_err, cpi->rc.kf_boost,
        cpi->twopass.kf_zeromotion_pct);

  fclose(f);

  if (0) {
    FILE *const fmodes = fopen("Modes.stt", "a");
    int i;

    fprintf(fmodes, "%6d:%1d:%1d:%1d ", cpi->common.current_video_frame,
            cm->frame_type, cpi->refresh_golden_frame,
            cpi->refresh_alt_ref_frame);

    for (i = 0; i < MAX_MODES; ++i)
      fprintf(fmodes, "%5d ", cpi->mode_chosen_counts[i]);

    fprintf(fmodes, "\n");

    fclose(fmodes);
  }
}
#endif

static void encode_without_recode_loop(VP9_COMP *cpi,
                                       int q) {
  VP9_COMMON *const cm = &cpi->common;
  vp9_clear_system_state();
  vp9_set_quantizer(cm, q);
  setup_frame(cpi);
  // Variance adaptive and in frame q adjustment experiments are mutually
  // exclusive.
  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    vp9_vaq_frame_setup(cpi);
  } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
    vp9_setup_in_frame_q_adj(cpi);
  } else if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
    vp9_cyclic_refresh_setup(cpi);
  }
  // transform / motion compensation build reconstruction frame
  vp9_encode_frame(cpi);

  // Update the skip mb flag probabilities based on the distribution
  // seen in the last encoder iteration.
  // update_base_skip_probs(cpi);
  vp9_clear_system_state();
}

static void encode_with_recode_loop(VP9_COMP *cpi,
                                    size_t *size,
                                    uint8_t *dest,
                                    int q,
                                    int bottom_index,
                                    int top_index) {
  VP9_COMMON *const cm = &cpi->common;
  RATE_CONTROL *const rc = &cpi->rc;
  int loop_count = 0;
  int loop = 0;
  int overshoot_seen = 0;
  int undershoot_seen = 0;
  int q_low = bottom_index, q_high = top_index;
  int frame_over_shoot_limit;
  int frame_under_shoot_limit;

  // Decide frame size bounds
  vp9_rc_compute_frame_size_bounds(cpi, rc->this_frame_target,
                                   &frame_under_shoot_limit,
                                   &frame_over_shoot_limit);

  do {
    vp9_clear_system_state();

    vp9_set_quantizer(cm, q);

    if (loop_count == 0)
      setup_frame(cpi);

    // Variance adaptive and in frame q adjustment experiments are mutually
    // exclusive.
    if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
      vp9_vaq_frame_setup(cpi);
    } else if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      vp9_setup_in_frame_q_adj(cpi);
    }

    // transform / motion compensation build reconstruction frame
    vp9_encode_frame(cpi);

    // Update the skip mb flag probabilities based on the distribution
    // seen in the last encoder iteration.
    // update_base_skip_probs(cpi);

    vp9_clear_system_state();

    // Dummy pack of the bitstream using up to date stats to get an
    // accurate estimate of output frame size to determine if we need
    // to recode.
    if (cpi->sf.recode_loop >= ALLOW_RECODE_KFARFGF) {
      save_coding_context(cpi);
      cpi->dummy_packing = 1;
      if (!cpi->sf.use_nonrd_pick_mode)
        vp9_pack_bitstream(cpi, dest, size);

      rc->projected_frame_size = (int)(*size) << 3;
      restore_coding_context(cpi);

      if (frame_over_shoot_limit == 0)
        frame_over_shoot_limit = 1;
    }

    if (cpi->oxcf.rc_mode == RC_MODE_CONSTANT_QUALITY) {
      loop = 0;
    } else {
      if ((cm->frame_type == KEY_FRAME) &&
           rc->this_key_frame_forced &&
           (rc->projected_frame_size < rc->max_frame_bandwidth)) {
        int last_q = q;
        int kf_err = vp9_get_y_sse(cpi->Source, get_frame_new_buffer(cm));

        int high_err_target = cpi->ambient_err;
        int low_err_target = cpi->ambient_err >> 1;

        // Prevent possible divide by zero error below for perfect KF
        kf_err += !kf_err;

        // The key frame is not good enough or we can afford
        // to make it better without undue risk of popping.
        if ((kf_err > high_err_target &&
             rc->projected_frame_size <= frame_over_shoot_limit) ||
            (kf_err > low_err_target &&
             rc->projected_frame_size <= frame_under_shoot_limit)) {
          // Lower q_high
          q_high = q > q_low ? q - 1 : q_low;

          // Adjust Q
          q = (q * high_err_target) / kf_err;
          q = MIN(q, (q_high + q_low) >> 1);
        } else if (kf_err < low_err_target &&
                   rc->projected_frame_size >= frame_under_shoot_limit) {
          // The key frame is much better than the previous frame
          // Raise q_low
          q_low = q < q_high ? q + 1 : q_high;

          // Adjust Q
          q = (q * low_err_target) / kf_err;
          q = MIN(q, (q_high + q_low + 1) >> 1);
        }

        // Clamp Q to upper and lower limits:
        q = clamp(q, q_low, q_high);

        loop = q != last_q;
      } else if (recode_loop_test(
          cpi, frame_over_shoot_limit, frame_under_shoot_limit,
          q, MAX(q_high, top_index), bottom_index)) {
        // Is the projected frame size out of range and are we allowed
        // to attempt to recode.
        int last_q = q;
        int retries = 0;

        // Frame size out of permitted range:
        // Update correction factor & compute new Q to try...

        // Frame is too large
        if (rc->projected_frame_size > rc->this_frame_target) {
          // Special case if the projected size is > the max allowed.
          if (rc->projected_frame_size >= rc->max_frame_bandwidth)
            q_high = rc->worst_quality;

          // Raise Qlow as to at least the current value
          q_low = q < q_high ? q + 1 : q_high;

          if (undershoot_seen || loop_count > 1) {
            // Update rate_correction_factor unless
            vp9_rc_update_rate_correction_factors(cpi, 1);

            q = (q_high + q_low + 1) / 2;
          } else {
            // Update rate_correction_factor unless
            vp9_rc_update_rate_correction_factors(cpi, 0);

            q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                                   bottom_index, MAX(q_high, top_index));

            while (q < q_low && retries < 10) {
              vp9_rc_update_rate_correction_factors(cpi, 0);
              q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                                     bottom_index, MAX(q_high, top_index));
              retries++;
            }
          }

          overshoot_seen = 1;
        } else {
          // Frame is too small
          q_high = q > q_low ? q - 1 : q_low;

          if (overshoot_seen || loop_count > 1) {
            vp9_rc_update_rate_correction_factors(cpi, 1);
            q = (q_high + q_low) / 2;
          } else {
            vp9_rc_update_rate_correction_factors(cpi, 0);
            q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                                   bottom_index, top_index);
            // Special case reset for qlow for constrained quality.
            // This should only trigger where there is very substantial
            // undershoot on a frame and the auto cq level is above
            // the user passsed in value.
            if (cpi->oxcf.rc_mode == RC_MODE_CONSTRAINED_QUALITY &&
                q < q_low) {
              q_low = q;
            }

            while (q > q_high && retries < 10) {
              vp9_rc_update_rate_correction_factors(cpi, 0);
              q = vp9_rc_regulate_q(cpi, rc->this_frame_target,
                                     bottom_index, top_index);
              retries++;
            }
          }

          undershoot_seen = 1;
        }

        // Clamp Q to upper and lower limits:
        q = clamp(q, q_low, q_high);

        loop = q != last_q;
      } else {
        loop = 0;
      }
    }

    // Special case for overlay frame.
    if (rc->is_src_frame_alt_ref &&
        rc->projected_frame_size < rc->max_frame_bandwidth)
      loop = 0;

    if (loop) {
      loop_count++;

#if CONFIG_INTERNAL_STATS
      cpi->tot_recode_hits++;
#endif
    }
  } while (loop);
}

static void get_ref_frame_flags(VP9_COMP *cpi) {
  if (cpi->refresh_last_frame & cpi->refresh_golden_frame)
    cpi->gold_is_last = 1;
  else if (cpi->refresh_last_frame ^ cpi->refresh_golden_frame)
    cpi->gold_is_last = 0;

  if (cpi->refresh_last_frame & cpi->refresh_alt_ref_frame)
    cpi->alt_is_last = 1;
  else if (cpi->refresh_last_frame ^ cpi->refresh_alt_ref_frame)
    cpi->alt_is_last = 0;

  if (cpi->refresh_alt_ref_frame & cpi->refresh_golden_frame)
    cpi->gold_is_alt = 1;
  else if (cpi->refresh_alt_ref_frame ^ cpi->refresh_golden_frame)
    cpi->gold_is_alt = 0;

  cpi->ref_frame_flags = VP9_ALT_FLAG | VP9_GOLD_FLAG | VP9_LAST_FLAG;

  if (cpi->gold_is_last)
    cpi->ref_frame_flags &= ~VP9_GOLD_FLAG;

  if (cpi->rc.frames_till_gf_update_due == INT_MAX)
    cpi->ref_frame_flags &= ~VP9_GOLD_FLAG;

  if (cpi->alt_is_last)
    cpi->ref_frame_flags &= ~VP9_ALT_FLAG;

  if (cpi->gold_is_alt)
    cpi->ref_frame_flags &= ~VP9_ALT_FLAG;
}

static void set_ext_overrides(VP9_COMP *cpi) {
  // Overrides the defaults with the externally supplied values with
  // vp9_update_reference() and vp9_update_entropy() calls
  // Note: The overrides are valid only for the next frame passed
  // to encode_frame_to_data_rate() function
  if (cpi->ext_refresh_frame_context_pending) {
    cpi->common.refresh_frame_context = cpi->ext_refresh_frame_context;
    cpi->ext_refresh_frame_context_pending = 0;
  }
  if (cpi->ext_refresh_frame_flags_pending) {
    cpi->refresh_last_frame = cpi->ext_refresh_last_frame;
    cpi->refresh_golden_frame = cpi->ext_refresh_golden_frame;
    cpi->refresh_alt_ref_frame = cpi->ext_refresh_alt_ref_frame;
    cpi->ext_refresh_frame_flags_pending = 0;
  }
}

YV12_BUFFER_CONFIG *vp9_scale_if_required(VP9_COMMON *cm,
                                          YV12_BUFFER_CONFIG *unscaled,
                                          YV12_BUFFER_CONFIG *scaled) {
  if (cm->mi_cols * MI_SIZE != unscaled->y_width ||
      cm->mi_rows * MI_SIZE != unscaled->y_height) {
    scale_and_extend_frame_nonnormative(unscaled, scaled);
    return scaled;
  } else {
    return unscaled;
  }
}

static void encode_frame_to_data_rate(VP9_COMP *cpi,
                                      size_t *size,
                                      uint8_t *dest,
                                      unsigned int *frame_flags) {
  VP9_COMMON *const cm = &cpi->common;
  TX_SIZE t;
  int q;
  int top_index;
  int bottom_index;

  const SPEED_FEATURES *const sf = &cpi->sf;
  const unsigned int max_mv_def = MIN(cm->width, cm->height);
  struct segmentation *const seg = &cm->seg;
  set_ext_overrides(cpi);

  cpi->Source = vp9_scale_if_required(cm, cpi->un_scaled_source,
                                      &cpi->scaled_source);

  if (cpi->unscaled_last_source != NULL)
    cpi->Last_Source = vp9_scale_if_required(cm, cpi->unscaled_last_source,
                                             &cpi->scaled_last_source);

  vp9_scale_references(cpi);

  vp9_clear_system_state();

  // Enable or disable mode based tweaking of the zbin.
  // For 2 pass only used where GF/ARF prediction quality
  // is above a threshold.
  cpi->zbin_mode_boost = 0;
  cpi->zbin_mode_boost_enabled = 0;

  // Current default encoder behavior for the altref sign bias.
  cm->ref_frame_sign_bias[ALTREF_FRAME] = cpi->rc.source_alt_ref_active;

  // Set default state for segment based loop filter update flags.
  cm->lf.mode_ref_delta_update = 0;

  // Initialize cpi->mv_step_param to default based on max resolution.
  cpi->mv_step_param = vp9_init_search_range(sf, max_mv_def);
  // Initialize cpi->max_mv_magnitude and cpi->mv_step_param if appropriate.
  if (sf->auto_mv_step_size) {
    if (frame_is_intra_only(cm)) {
      // Initialize max_mv_magnitude for use in the first INTER frame
      // after a key/intra-only frame.
      cpi->max_mv_magnitude = max_mv_def;
    } else {
      if (cm->show_frame)
        // Allow mv_steps to correspond to twice the max mv magnitude found
        // in the previous frame, capped by the default max_mv_magnitude based
        // on resolution.
        cpi->mv_step_param = vp9_init_search_range(sf, MIN(max_mv_def, 2 *
                                 cpi->max_mv_magnitude));
      cpi->max_mv_magnitude = 0;
    }
  }

  // Set various flags etc to special state if it is a key frame.
  if (frame_is_intra_only(cm)) {
    // Reset the loop filter deltas and segmentation map.
    vp9_reset_segment_features(&cm->seg);

    // If segmentation is enabled force a map update for key frames.
    if (seg->enabled) {
      seg->update_map = 1;
      seg->update_data = 1;
    }

    // The alternate reference frame cannot be active for a key frame.
    cpi->rc.source_alt_ref_active = 0;

    cm->error_resilient_mode = (cpi->oxcf.error_resilient_mode != 0);
    cm->frame_parallel_decoding_mode =
      (cpi->oxcf.frame_parallel_decoding_mode != 0);

    // By default, encoder assumes decoder can use prev_mi.
    cm->coding_use_prev_mi = 1;
    if (cm->error_resilient_mode) {
      cm->coding_use_prev_mi = 0;
      cm->frame_parallel_decoding_mode = 1;
      cm->reset_frame_context = 0;
      cm->refresh_frame_context = 0;
    } else if (cm->intra_only) {
      // Only reset the current context.
      cm->reset_frame_context = 2;
    }
  }

  // Configure experimental use of segmentation for enhanced coding of
  // static regions if indicated.
  // Only allowed in second pass of two pass (as requires lagged coding)
  // and if the relevant speed feature flag is set.
  if (cpi->pass == 2 && cpi->sf.static_segmentation)
    configure_static_seg_features(cpi);

  // For 1 pass CBR, check if we are dropping this frame.
  // Never drop on key frame.
  if (cpi->pass == 0 &&
      cpi->oxcf.rc_mode == RC_MODE_CBR &&
      cm->frame_type != KEY_FRAME) {
    if (vp9_rc_drop_frame(cpi)) {
      vp9_rc_postencode_update_drop_frame(cpi);
      ++cm->current_video_frame;
      return;
    }
  }

  vp9_clear_system_state();

  vp9_zero(cpi->rd.tx_select_threshes);

#if CONFIG_VP9_POSTPROC
  if (cpi->oxcf.noise_sensitivity > 0) {
    int l = 0;
    switch (cpi->oxcf.noise_sensitivity) {
      case 1:
        l = 20;
        break;
      case 2:
        l = 40;
        break;
      case 3:
        l = 60;
        break;
      case 4:
      case 5:
        l = 100;
        break;
      case 6:
        l = 150;
        break;
    }
    vp9_denoise(cpi->Source, cpi->Source, l);
  }
#endif

#ifdef OUTPUT_YUV_SRC
  vp9_write_yuv_frame(cpi->Source);
#endif

  set_speed_features(cpi);

  // Decide q and q bounds.
  q = vp9_rc_pick_q_and_bounds(cpi, &bottom_index, &top_index);

  if (!frame_is_intra_only(cm)) {
    cm->interp_filter = DEFAULT_INTERP_FILTER;
    /* TODO: Decide this more intelligently */
    set_high_precision_mv(cpi, q < HIGH_PRECISION_MV_QTHRESH);
  }

  if (cpi->sf.recode_loop == DISALLOW_RECODE) {
    encode_without_recode_loop(cpi, q);
  } else {
    encode_with_recode_loop(cpi, size, dest, q, bottom_index, top_index);
  }

  // Special case code to reduce pulsing when key frames are forced at a
  // fixed interval. Note the reconstruction error if it is the frame before
  // the force key frame
  if (cpi->rc.next_key_frame_forced && cpi->rc.frames_to_key == 1) {
    cpi->ambient_err = vp9_get_y_sse(cpi->Source, get_frame_new_buffer(cm));
  }

  // If the encoder forced a KEY_FRAME decision
  if (cm->frame_type == KEY_FRAME)
    cpi->refresh_last_frame = 1;

  cm->frame_to_show = get_frame_new_buffer(cm);

#if WRITE_RECON_BUFFER
  if (cm->show_frame)
    write_cx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame);
  else
    write_cx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 1000);
#endif

  // Pick the loop filter level for the frame.
  loopfilter_frame(cpi, cm);

#if WRITE_RECON_BUFFER
  if (cm->show_frame)
    write_cx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 2000);
  else
    write_cx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 3000);
#endif

  // build the bitstream
  cpi->dummy_packing = 0;
  vp9_pack_bitstream(cpi, dest, size);

  if (cm->seg.update_map)
    update_reference_segmentation_map(cpi);

  release_scaled_references(cpi);
  vp9_update_reference_frames(cpi);

  for (t = TX_4X4; t <= TX_32X32; t++)
    full_to_model_counts(cm->counts.coef[t], cpi->coef_counts[t]);

  if (!cm->error_resilient_mode && !cm->frame_parallel_decoding_mode)
    vp9_adapt_coef_probs(cm);

  if (!frame_is_intra_only(cm)) {
    if (!cm->error_resilient_mode && !cm->frame_parallel_decoding_mode) {
      vp9_adapt_mode_probs(cm);
      vp9_adapt_mv_probs(cm, cm->allow_high_precision_mv);
    }
  }

  if (cpi->refresh_golden_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_GOLDEN;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_GOLDEN;

  if (cpi->refresh_alt_ref_frame == 1)
    cpi->frame_flags |= FRAMEFLAGS_ALTREF;
  else
    cpi->frame_flags &= ~FRAMEFLAGS_ALTREF;

  get_ref_frame_flags(cpi);

  cm->last_frame_type = cm->frame_type;
  vp9_rc_postencode_update(cpi, *size);

#if 0
  output_frame_level_debug_stats(cpi);
#endif

  if (cm->frame_type == KEY_FRAME) {
    // Tell the caller that the frame was coded as a key frame
    *frame_flags = cpi->frame_flags | FRAMEFLAGS_KEY;

#if CONFIG_MULTIPLE_ARF
    // Reset the sequence number.
    if (cpi->multi_arf_enabled) {
      cpi->sequence_number = 0;
      cpi->frame_coding_order_period = cpi->new_frame_coding_order_period;
      cpi->new_frame_coding_order_period = -1;
    }
#endif
  } else {
    *frame_flags = cpi->frame_flags & ~FRAMEFLAGS_KEY;

#if CONFIG_MULTIPLE_ARF
    /* Increment position in the coded frame sequence. */
    if (cpi->multi_arf_enabled) {
      ++cpi->sequence_number;
      if (cpi->sequence_number >= cpi->frame_coding_order_period) {
        cpi->sequence_number = 0;
        cpi->frame_coding_order_period = cpi->new_frame_coding_order_period;
        cpi->new_frame_coding_order_period = -1;
      }
      cpi->this_frame_weight = cpi->arf_weight[cpi->sequence_number];
      assert(cpi->this_frame_weight >= 0);
    }
#endif
  }

  // Clear the one shot update flags for segmentation map and mode/ref loop
  // filter deltas.
  cm->seg.update_map = 0;
  cm->seg.update_data = 0;
  cm->lf.mode_ref_delta_update = 0;

  // keep track of the last coded dimensions
  cm->last_width = cm->width;
  cm->last_height = cm->height;

  // reset to normal state now that we are done.
  if (!cm->show_existing_frame)
    cm->last_show_frame = cm->show_frame;

  if (cm->show_frame) {
    vp9_swap_mi_and_prev_mi(cm);

    // Don't increment frame counters if this was an altref buffer
    // update not a real frame
    ++cm->current_video_frame;
    if (cpi->use_svc)
      vp9_inc_frame_in_layer(&cpi->svc);
  }
}

static void SvcEncode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                      unsigned int *frame_flags) {
  vp9_rc_get_svc_params(cpi);
  encode_frame_to_data_rate(cpi, size, dest, frame_flags);
}

static void Pass0Encode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                        unsigned int *frame_flags) {
  if (cpi->oxcf.rc_mode == RC_MODE_CBR) {
    vp9_rc_get_one_pass_cbr_params(cpi);
  } else {
    vp9_rc_get_one_pass_vbr_params(cpi);
  }
  encode_frame_to_data_rate(cpi, size, dest, frame_flags);
}

static void Pass1Encode(VP9_COMP *cpi, size_t *size, uint8_t *dest,
                        unsigned int *frame_flags) {
  (void) size;
  (void) dest;
  (void) frame_flags;

  vp9_rc_get_first_pass_params(cpi);
  vp9_set_quantizer(&cpi->common, find_fp_qindex());
  vp9_first_pass(cpi);
}

static void Pass2Encode(VP9_COMP *cpi, size_t *size,
                        uint8_t *dest, unsigned int *frame_flags) {
  cpi->allow_encode_breakout = ENCODE_BREAKOUT_ENABLED;

  vp9_rc_get_second_pass_params(cpi);
  encode_frame_to_data_rate(cpi, size, dest, frame_flags);

  vp9_twopass_postencode_update(cpi);
}

static void check_initial_width(VP9_COMP *cpi, int subsampling_x,
                                int subsampling_y) {
  VP9_COMMON *const cm = &cpi->common;

  if (!cpi->initial_width) {
    cm->subsampling_x = subsampling_x;
    cm->subsampling_y = subsampling_y;
    alloc_raw_frame_buffers(cpi);
    cpi->initial_width = cm->width;
    cpi->initial_height = cm->height;
  }
}


int vp9_receive_raw_frame(VP9_COMP *cpi, unsigned int frame_flags,
                          YV12_BUFFER_CONFIG *sd, int64_t time_stamp,
                          int64_t end_time) {
  VP9_COMMON *cm = &cpi->common;
  struct vpx_usec_timer timer;
  int res = 0;
  const int subsampling_x = sd->uv_width  < sd->y_width;
  const int subsampling_y = sd->uv_height < sd->y_height;

  check_initial_width(cpi, subsampling_x, subsampling_y);
  vpx_usec_timer_start(&timer);
  if (vp9_lookahead_push(cpi->lookahead,
                         sd, time_stamp, end_time, frame_flags))
    res = -1;
  vpx_usec_timer_mark(&timer);
  cpi->time_receive_data += vpx_usec_timer_elapsed(&timer);

  if (cm->profile == PROFILE_0 && (subsampling_x != 1 || subsampling_y != 1)) {
    vpx_internal_error(&cm->error, VPX_CODEC_INVALID_PARAM,
                       "Non-4:2:0 color space requires profile >= 1");
    res = -1;
  }

  return res;
}


static int frame_is_reference(const VP9_COMP *cpi) {
  const VP9_COMMON *cm = &cpi->common;

  return cm->frame_type == KEY_FRAME ||
         cpi->refresh_last_frame ||
         cpi->refresh_golden_frame ||
         cpi->refresh_alt_ref_frame ||
         cm->refresh_frame_context ||
         cm->lf.mode_ref_delta_update ||
         cm->seg.update_map ||
         cm->seg.update_data;
}

#if CONFIG_MULTIPLE_ARF
int is_next_frame_arf(VP9_COMP *cpi) {
  // Negative entry in frame_coding_order indicates an ARF at this position.
  return cpi->frame_coding_order[cpi->sequence_number + 1] < 0 ? 1 : 0;
}
#endif

void adjust_frame_rate(VP9_COMP *cpi) {
  int64_t this_duration;
  int step = 0;

  if (cpi->source->ts_start == cpi->first_time_stamp_ever) {
    this_duration = cpi->source->ts_end - cpi->source->ts_start;
    step = 1;
  } else {
    int64_t last_duration = cpi->last_end_time_stamp_seen
        - cpi->last_time_stamp_seen;

    this_duration = cpi->source->ts_end - cpi->last_end_time_stamp_seen;

    // do a step update if the duration changes by 10%
    if (last_duration)
      step = (int)((this_duration - last_duration) * 10 / last_duration);
  }

  if (this_duration) {
    if (step) {
      vp9_new_framerate(cpi, 10000000.0 / this_duration);
    } else {
      // Average this frame's rate into the last second's average
      // frame rate. If we haven't seen 1 second yet, then average
      // over the whole interval seen.
      const double interval = MIN((double)(cpi->source->ts_end
                                   - cpi->first_time_stamp_ever), 10000000.0);
      double avg_duration = 10000000.0 / cpi->oxcf.framerate;
      avg_duration *= (interval - avg_duration + this_duration);
      avg_duration /= interval;

      vp9_new_framerate(cpi, 10000000.0 / avg_duration);
    }
  }
  cpi->last_time_stamp_seen = cpi->source->ts_start;
  cpi->last_end_time_stamp_seen = cpi->source->ts_end;
}

int vp9_get_compressed_data(VP9_COMP *cpi, unsigned int *frame_flags,
                            size_t *size, uint8_t *dest,
                            int64_t *time_stamp, int64_t *time_end, int flush) {
  VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &cpi->mb.e_mbd;
  RATE_CONTROL *const rc = &cpi->rc;
  struct vpx_usec_timer  cmptimer;
  YV12_BUFFER_CONFIG *force_src_buffer = NULL;
  MV_REFERENCE_FRAME ref_frame;

  if (!cpi)
    return -1;

  if (cpi->svc.number_spatial_layers > 1 && cpi->pass == 2) {
    vp9_restore_layer_context(cpi);
  }

  vpx_usec_timer_start(&cmptimer);

  cpi->source = NULL;
  cpi->last_source = NULL;

  set_high_precision_mv(cpi, ALTREF_HIGH_PRECISION_MV);

  // Normal defaults
  cm->reset_frame_context = 0;
  cm->refresh_frame_context = 1;
  cpi->refresh_last_frame = 1;
  cpi->refresh_golden_frame = 0;
  cpi->refresh_alt_ref_frame = 0;

  // Should we code an alternate reference frame.
  if (cpi->oxcf.play_alternate && rc->source_alt_ref_pending) {
    int frames_to_arf;

#if CONFIG_MULTIPLE_ARF
    assert(!cpi->multi_arf_enabled ||
           cpi->frame_coding_order[cpi->sequence_number] < 0);

    if (cpi->multi_arf_enabled && (cpi->pass == 2))
      frames_to_arf = (-cpi->frame_coding_order[cpi->sequence_number])
          - cpi->next_frame_in_order;
    else
#endif
      frames_to_arf = rc->frames_till_gf_update_due;

    assert(frames_to_arf <= rc->frames_to_key);

    if ((cpi->source = vp9_lookahead_peek(cpi->lookahead, frames_to_arf))) {
#if CONFIG_MULTIPLE_ARF
      cpi->alt_ref_source[cpi->arf_buffered] = cpi->source;
#else
      cpi->alt_ref_source = cpi->source;
#endif

      if (cpi->oxcf.arnr_max_frames > 0) {
        // Produce the filtered ARF frame.
        // TODO(agrange) merge these two functions.
        vp9_configure_arnr_filter(cpi, frames_to_arf, rc->gfu_boost);
        vp9_temporal_filter_prepare(cpi, frames_to_arf);
        vp9_extend_frame_borders(&cpi->alt_ref_buffer);
        force_src_buffer = &cpi->alt_ref_buffer;
      }

      cm->show_frame = 0;
      cpi->refresh_alt_ref_frame = 1;
      cpi->refresh_golden_frame = 0;
      cpi->refresh_last_frame = 0;
      rc->is_src_frame_alt_ref = 0;

#if CONFIG_MULTIPLE_ARF
      if (!cpi->multi_arf_enabled)
#endif
        rc->source_alt_ref_pending = 0;
    } else {
      rc->source_alt_ref_pending = 0;
    }
  }

  if (!cpi->source) {
#if CONFIG_MULTIPLE_ARF
    int i;
#endif

    // Get last frame source.
    if (cm->current_video_frame > 0) {
      if ((cpi->last_source = vp9_lookahead_peek(cpi->lookahead, -1)) == NULL)
        return -1;
    }

    if ((cpi->source = vp9_lookahead_pop(cpi->lookahead, flush))) {
      cm->show_frame = 1;
      cm->intra_only = 0;

#if CONFIG_MULTIPLE_ARF
      // Is this frame the ARF overlay.
      rc->is_src_frame_alt_ref = 0;
      for (i = 0; i < cpi->arf_buffered; ++i) {
        if (cpi->source == cpi->alt_ref_source[i]) {
          rc->is_src_frame_alt_ref = 1;
          cpi->refresh_golden_frame = 1;
          break;
        }
      }
#else
      rc->is_src_frame_alt_ref = cpi->alt_ref_source &&
                                 (cpi->source == cpi->alt_ref_source);
#endif
      if (rc->is_src_frame_alt_ref) {
        // Current frame is an ARF overlay frame.
#if CONFIG_MULTIPLE_ARF
        cpi->alt_ref_source[i] = NULL;
#else
        cpi->alt_ref_source = NULL;
#endif
        // Don't refresh the last buffer for an ARF overlay frame. It will
        // become the GF so preserve last as an alternative prediction option.
        cpi->refresh_last_frame = 0;
      }
#if CONFIG_MULTIPLE_ARF
      ++cpi->next_frame_in_order;
#endif
    }
  }

  if (cpi->source) {
    cpi->un_scaled_source = cpi->Source = force_src_buffer ? force_src_buffer
                                                           : &cpi->source->img;

  if (cpi->last_source != NULL) {
    cpi->unscaled_last_source = &cpi->last_source->img;
  } else {
    cpi->unscaled_last_source = NULL;
  }

    *time_stamp = cpi->source->ts_start;
    *time_end = cpi->source->ts_end;
    *frame_flags = cpi->source->flags;

#if CONFIG_MULTIPLE_ARF
    if (cm->frame_type != KEY_FRAME && cpi->pass == 2)
      rc->source_alt_ref_pending = is_next_frame_arf(cpi);
#endif
  } else {
    *size = 0;
    if (flush && cpi->pass == 1 && !cpi->twopass.first_pass_done) {
      vp9_end_first_pass(cpi);    /* get last stats packet */
      cpi->twopass.first_pass_done = 1;
    }
    return -1;
  }

  if (cpi->source->ts_start < cpi->first_time_stamp_ever) {
    cpi->first_time_stamp_ever = cpi->source->ts_start;
    cpi->last_end_time_stamp_seen = cpi->source->ts_start;
  }

  // adjust frame rates based on timestamps given
  if (cm->show_frame) {
    adjust_frame_rate(cpi);
  }

  if (cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.rc_mode == RC_MODE_CBR) {
    vp9_update_temporal_layer_framerate(cpi);
    vp9_restore_layer_context(cpi);
  }

  // start with a 0 size frame
  *size = 0;

  // Clear down mmx registers
  vp9_clear_system_state();

  /* find a free buffer for the new frame, releasing the reference previously
   * held.
   */
  cm->frame_bufs[cm->new_fb_idx].ref_count--;
  cm->new_fb_idx = get_free_fb(cm);

#if CONFIG_MULTIPLE_ARF
  /* Set up the correct ARF frame. */
  if (cpi->refresh_alt_ref_frame) {
    ++cpi->arf_buffered;
  }
  if (cpi->multi_arf_enabled && (cm->frame_type != KEY_FRAME) &&
      (cpi->pass == 2)) {
    cpi->alt_fb_idx = cpi->arf_buffer_idx[cpi->sequence_number];
  }
#endif

  cpi->frame_flags = *frame_flags;

  if (cpi->pass == 2 &&
      cm->current_video_frame == 0 &&
      cpi->oxcf.allow_spatial_resampling &&
      cpi->oxcf.rc_mode == RC_MODE_VBR) {
    // Internal scaling is triggered on the first frame.
    vp9_set_size_literal(cpi, cpi->oxcf.scaled_frame_width,
                         cpi->oxcf.scaled_frame_height);
  }

  // Reset the frame pointers to the current frame size
  vp9_realloc_frame_buffer(get_frame_new_buffer(cm),
                           cm->width, cm->height,
                           cm->subsampling_x, cm->subsampling_y,
                           VP9_ENC_BORDER_IN_PIXELS, NULL, NULL, NULL);

  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    const int idx = cm->ref_frame_map[get_ref_frame_idx(cpi, ref_frame)];
    YV12_BUFFER_CONFIG *const buf = &cm->frame_bufs[idx].buf;
    RefBuffer *const ref_buf = &cm->frame_refs[ref_frame - 1];
    ref_buf->buf = buf;
    ref_buf->idx = idx;
    vp9_setup_scale_factors_for_frame(&ref_buf->sf,
                                      buf->y_crop_width, buf->y_crop_height,
                                      cm->width, cm->height);

    if (vp9_is_scaled(&ref_buf->sf))
      vp9_extend_frame_borders(buf);
  }

  set_ref_ptrs(cm, xd, LAST_FRAME, LAST_FRAME);

  if (cpi->oxcf.aq_mode == VARIANCE_AQ) {
    vp9_vaq_init();
  }

  if (cpi->pass == 1 &&
      (!cpi->use_svc || cpi->svc.number_temporal_layers == 1)) {
    Pass1Encode(cpi, size, dest, frame_flags);
  } else if (cpi->pass == 2 &&
      (!cpi->use_svc || cpi->svc.number_temporal_layers == 1)) {
    Pass2Encode(cpi, size, dest, frame_flags);
  } else if (cpi->use_svc) {
    SvcEncode(cpi, size, dest, frame_flags);
  } else {
    // One pass encode
    Pass0Encode(cpi, size, dest, frame_flags);
  }

  if (cm->refresh_frame_context)
    cm->frame_contexts[cm->frame_context_idx] = cm->fc;

  // Frame was dropped, release scaled references.
  if (*size == 0) {
    release_scaled_references(cpi);
  }

  if (*size > 0) {
    cpi->droppable = !frame_is_reference(cpi);
  }

  // Save layer specific state.
  if ((cpi->svc.number_temporal_layers > 1 &&
      cpi->oxcf.rc_mode == RC_MODE_CBR) ||
      (cpi->svc.number_spatial_layers > 1 && cpi->pass == 2)) {
    vp9_save_layer_context(cpi);
  }

  vpx_usec_timer_mark(&cmptimer);
  cpi->time_compress_data += vpx_usec_timer_elapsed(&cmptimer);

  if (cpi->b_calculate_psnr && cpi->pass != 1 && cm->show_frame)
    generate_psnr_packet(cpi);

#if CONFIG_INTERNAL_STATS

  if (cpi->pass != 1) {
    cpi->bytes += (int)(*size);

    if (cm->show_frame) {
      cpi->count++;

      if (cpi->b_calculate_psnr) {
        YV12_BUFFER_CONFIG *orig = cpi->Source;
        YV12_BUFFER_CONFIG *recon = cpi->common.frame_to_show;
        YV12_BUFFER_CONFIG *pp = &cm->post_proc_buffer;
        PSNR_STATS psnr;
        calc_psnr(orig, recon, &psnr);

        cpi->total += psnr.psnr[0];
        cpi->total_y += psnr.psnr[1];
        cpi->total_u += psnr.psnr[2];
        cpi->total_v += psnr.psnr[3];
        cpi->total_sq_error += psnr.sse[0];
        cpi->total_samples += psnr.samples[0];

        {
          PSNR_STATS psnr2;
          double frame_ssim2 = 0, weight = 0;
#if CONFIG_VP9_POSTPROC
          vp9_deblock(cm->frame_to_show, &cm->post_proc_buffer,
                      cm->lf.filter_level * 10 / 6);
#endif
          vp9_clear_system_state();

          calc_psnr(orig, pp, &psnr2);

          cpi->totalp += psnr2.psnr[0];
          cpi->totalp_y += psnr2.psnr[1];
          cpi->totalp_u += psnr2.psnr[2];
          cpi->totalp_v += psnr2.psnr[3];
          cpi->totalp_sq_error += psnr2.sse[0];
          cpi->totalp_samples += psnr2.samples[0];

          frame_ssim2 = vp9_calc_ssim(orig, recon, 1, &weight);

          cpi->summed_quality += frame_ssim2 * weight;
          cpi->summed_weights += weight;

          frame_ssim2 = vp9_calc_ssim(orig, &cm->post_proc_buffer, 1, &weight);

          cpi->summedp_quality += frame_ssim2 * weight;
          cpi->summedp_weights += weight;
#if 0
          {
            FILE *f = fopen("q_used.stt", "a");
            fprintf(f, "%5d : Y%f7.3:U%f7.3:V%f7.3:F%f7.3:S%7.3f\n",
                    cpi->common.current_video_frame, y2, u2, v2,
                    frame_psnr2, frame_ssim2);
            fclose(f);
          }
#endif
        }
      }

      if (cpi->b_calculate_ssimg) {
        double y, u, v, frame_all;
        frame_all = vp9_calc_ssimg(cpi->Source, cm->frame_to_show, &y, &u, &v);
        cpi->total_ssimg_y += y;
        cpi->total_ssimg_u += u;
        cpi->total_ssimg_v += v;
        cpi->total_ssimg_all += frame_all;
      }
    }
  }

#endif
  return 0;
}

int vp9_get_preview_raw_frame(VP9_COMP *cpi, YV12_BUFFER_CONFIG *dest,
                              vp9_ppflags_t *flags) {
  VP9_COMMON *cm = &cpi->common;
#if !CONFIG_VP9_POSTPROC
  (void)flags;
#endif

  if (!cm->show_frame) {
    return -1;
  } else {
    int ret;
#if CONFIG_VP9_POSTPROC
    ret = vp9_post_proc_frame(cm, dest, flags);
#else
    if (cm->frame_to_show) {
      *dest = *cm->frame_to_show;
      dest->y_width = cm->width;
      dest->y_height = cm->height;
      dest->uv_width = cm->width >> cm->subsampling_x;
      dest->uv_height = cm->height >> cm->subsampling_y;
      ret = 0;
    } else {
      ret = -1;
    }
#endif  // !CONFIG_VP9_POSTPROC
    vp9_clear_system_state();
    return ret;
  }
}

int vp9_set_active_map(VP9_COMP *cpi, unsigned char *map, int rows, int cols) {
  if (rows == cpi->common.mb_rows && cols == cpi->common.mb_cols) {
    if (map) {
      vpx_memcpy(cpi->active_map, map, rows * cols);
      cpi->active_map_enabled = 1;
    } else {
      cpi->active_map_enabled = 0;
    }

    return 0;
  } else {
    // cpi->active_map_enabled = 0;
    return -1;
  }
}

int vp9_set_internal_size(VP9_COMP *cpi,
                          VPX_SCALING horiz_mode, VPX_SCALING vert_mode) {
  VP9_COMMON *cm = &cpi->common;
  int hr = 0, hs = 0, vr = 0, vs = 0;

  if (horiz_mode > ONETWO || vert_mode > ONETWO)
    return -1;

  Scale2Ratio(horiz_mode, &hr, &hs);
  Scale2Ratio(vert_mode, &vr, &vs);

  // always go to the next whole number
  cm->width = (hs - 1 + cpi->oxcf.width * hr) / hs;
  cm->height = (vs - 1 + cpi->oxcf.height * vr) / vs;

  assert(cm->width <= cpi->initial_width);
  assert(cm->height <= cpi->initial_height);
  update_frame_size(cpi);
  return 0;
}

int vp9_set_size_literal(VP9_COMP *cpi, unsigned int width,
                         unsigned int height) {
  VP9_COMMON *cm = &cpi->common;

  check_initial_width(cpi, 1, 1);

  if (width) {
    cm->width = width;
    if (cm->width * 5 < cpi->initial_width) {
      cm->width = cpi->initial_width / 5 + 1;
      printf("Warning: Desired width too small, changed to %d\n", cm->width);
    }
    if (cm->width > cpi->initial_width) {
      cm->width = cpi->initial_width;
      printf("Warning: Desired width too large, changed to %d\n", cm->width);
    }
  }

  if (height) {
    cm->height = height;
    if (cm->height * 5 < cpi->initial_height) {
      cm->height = cpi->initial_height / 5 + 1;
      printf("Warning: Desired height too small, changed to %d\n", cm->height);
    }
    if (cm->height > cpi->initial_height) {
      cm->height = cpi->initial_height;
      printf("Warning: Desired height too large, changed to %d\n", cm->height);
    }
  }

  assert(cm->width <= cpi->initial_width);
  assert(cm->height <= cpi->initial_height);
  update_frame_size(cpi);
  return 0;
}

void vp9_set_svc(VP9_COMP *cpi, int use_svc) {
  cpi->use_svc = use_svc;
  return;
}

int vp9_get_y_sse(const YV12_BUFFER_CONFIG *a, const YV12_BUFFER_CONFIG *b) {
  assert(a->y_crop_width == b->y_crop_width);
  assert(a->y_crop_height == b->y_crop_height);

  return (int)get_sse(a->y_buffer, a->y_stride, b->y_buffer, b->y_stride,
                      a->y_crop_width, a->y_crop_height);
}


int vp9_get_quantizer(VP9_COMP *cpi) {
  return cpi->common.base_qindex;
}
