/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include "vpx/vpx_codec.h"
#include "vpx/internal/vpx_codec_internal.h"
#include "./vpx_version.h"
#include "vp9/encoder/vp9_encoder.h"
#include "vpx/vp8cx.h"
#include "vp9/encoder/vp9_firstpass.h"
#include "vp9/vp9_iface_common.h"

struct vp9_extracfg {
  struct vpx_codec_pkt_list *pkt_list;
  int                         cpu_used;  // available cpu percentage in 1/16
  unsigned int                enable_auto_alt_ref;
  unsigned int                noise_sensitivity;
  unsigned int                sharpness;
  unsigned int                static_thresh;
  unsigned int                tile_columns;
  unsigned int                tile_rows;
  unsigned int                arnr_max_frames;
  unsigned int                arnr_strength;
  unsigned int                arnr_type;
  vp8e_tuning                 tuning;
  unsigned int                cq_level;  // constrained quality level
  unsigned int                rc_max_intra_bitrate_pct;
  unsigned int                lossless;
  unsigned int                frame_parallel_decoding_mode;
  AQ_MODE                     aq_mode;
  unsigned int                frame_periodic_boost;
  BIT_DEPTH                   bit_depth;
};

struct extraconfig_map {
  unsigned int usage;
  struct vp9_extracfg cfg;
};

static const struct extraconfig_map extracfg_map[] = {
  {
    0,
    { // NOLINT
      NULL,
      0,                          // cpu_used
      1,                          // enable_auto_alt_ref
      0,                          // noise_sensitivity
      0,                          // sharpness
      0,                          // static_thresh
      0,                          // tile_columns
      0,                          // tile_rows
      7,                          // arnr_max_frames
      5,                          // arnr_strength
      3,                          // arnr_type
      VP8_TUNE_PSNR,              // tuning
      10,                         // cq_level
      0,                          // rc_max_intra_bitrate_pct
      0,                          // lossless
      0,                          // frame_parallel_decoding_mode
      NO_AQ,                      // aq_mode
      0,                          // frame_periodic_delta_q
      BITS_8,                     // Bit depth
    }
  }
};

struct vpx_codec_alg_priv {
  vpx_codec_priv_t        base;
  vpx_codec_enc_cfg_t     cfg;
  struct vp9_extracfg     extra_cfg;
  VP9EncoderConfig        oxcf;
  VP9_COMP               *cpi;
  unsigned char          *cx_data;
  size_t                  cx_data_sz;
  unsigned char          *pending_cx_data;
  size_t                  pending_cx_data_sz;
  int                     pending_frame_count;
  size_t                  pending_frame_sizes[8];
  size_t                  pending_frame_magnitude;
  vpx_image_t             preview_img;
  vp8_postproc_cfg_t      preview_ppcfg;
  vpx_codec_pkt_list_decl(64) pkt_list;
  unsigned int                fixed_kf_cntr;
};

static VP9_REFFRAME ref_frame_to_vp9_reframe(vpx_ref_frame_type_t frame) {
  switch (frame) {
    case VP8_LAST_FRAME:
      return VP9_LAST_FLAG;
    case VP8_GOLD_FRAME:
      return VP9_GOLD_FLAG;
    case VP8_ALTR_FRAME:
      return VP9_ALT_FLAG;
  }
  assert(0 && "Invalid Reference Frame");
  return VP9_LAST_FLAG;
}

static vpx_codec_err_t update_error_state(vpx_codec_alg_priv_t *ctx,
    const struct vpx_internal_error_info *error) {
  const vpx_codec_err_t res = error->error_code;

  if (res != VPX_CODEC_OK)
    ctx->base.err_detail = error->has_detail ? error->detail : NULL;

  return res;
}


#undef ERROR
#define ERROR(str) do {\
    ctx->base.err_detail = str;\
    return VPX_CODEC_INVALID_PARAM;\
  } while (0)

#define RANGE_CHECK(p, memb, lo, hi) do {\
    if (!(((p)->memb == lo || (p)->memb > (lo)) && (p)->memb <= hi)) \
      ERROR(#memb " out of range ["#lo".."#hi"]");\
  } while (0)

#define RANGE_CHECK_HI(p, memb, hi) do {\
    if (!((p)->memb <= (hi))) \
      ERROR(#memb " out of range [.."#hi"]");\
  } while (0)

#define RANGE_CHECK_LO(p, memb, lo) do {\
    if (!((p)->memb >= (lo))) \
      ERROR(#memb " out of range ["#lo"..]");\
  } while (0)

#define RANGE_CHECK_BOOL(p, memb) do {\
    if (!!((p)->memb) != (p)->memb) ERROR(#memb " expected boolean");\
  } while (0)

static vpx_codec_err_t validate_config(vpx_codec_alg_priv_t *ctx,
                                       const vpx_codec_enc_cfg_t *cfg,
                                       const struct vp9_extracfg *extra_cfg) {
  RANGE_CHECK(cfg, g_w,                   1, 65535);  // 16 bits available
  RANGE_CHECK(cfg, g_h,                   1, 65535);  // 16 bits available
  RANGE_CHECK(cfg, g_timebase.den,        1, 1000000000);
  RANGE_CHECK(cfg, g_timebase.num,        1, cfg->g_timebase.den);
  RANGE_CHECK_HI(cfg, g_profile,          3);

  RANGE_CHECK_HI(cfg, rc_max_quantizer,   63);
  RANGE_CHECK_HI(cfg, rc_min_quantizer,   cfg->rc_max_quantizer);
  RANGE_CHECK_BOOL(extra_cfg, lossless);
  RANGE_CHECK(extra_cfg, aq_mode,           0, AQ_MODE_COUNT - 1);
  RANGE_CHECK(extra_cfg, frame_periodic_boost, 0, 1);
  RANGE_CHECK_HI(cfg, g_threads,          64);
  RANGE_CHECK_HI(cfg, g_lag_in_frames,    MAX_LAG_BUFFERS);
  RANGE_CHECK(cfg, rc_end_usage,          VPX_VBR, VPX_Q);
  RANGE_CHECK_HI(cfg, rc_undershoot_pct,  1000);
  RANGE_CHECK_HI(cfg, rc_overshoot_pct,   1000);
  RANGE_CHECK_HI(cfg, rc_2pass_vbr_bias_pct, 100);
  RANGE_CHECK(cfg, kf_mode,               VPX_KF_DISABLED, VPX_KF_AUTO);
  RANGE_CHECK_BOOL(cfg,                   rc_resize_allowed);
  RANGE_CHECK_HI(cfg, rc_dropframe_thresh,   100);
  RANGE_CHECK_HI(cfg, rc_resize_up_thresh,   100);
  RANGE_CHECK_HI(cfg, rc_resize_down_thresh, 100);
  RANGE_CHECK(cfg,        g_pass,         VPX_RC_ONE_PASS, VPX_RC_LAST_PASS);

  if (cfg->rc_resize_allowed == 1) {
    RANGE_CHECK(cfg, rc_scaled_width, 1, cfg->g_w);
    RANGE_CHECK(cfg, rc_scaled_height, 1, cfg->g_h);
  }

  RANGE_CHECK(cfg, ss_number_layers, 1, VPX_SS_MAX_LAYERS);
  RANGE_CHECK(cfg, ts_number_layers, 1, VPX_TS_MAX_LAYERS);
  if (cfg->ts_number_layers > 1) {
    unsigned int i;
    for (i = 1; i < cfg->ts_number_layers; ++i)
      if (cfg->ts_target_bitrate[i] < cfg->ts_target_bitrate[i - 1])
        ERROR("ts_target_bitrate entries are not increasing");

    RANGE_CHECK(cfg, ts_rate_decimator[cfg->ts_number_layers - 1], 1, 1);
    for (i = cfg->ts_number_layers - 2; i > 0; --i)
      if (cfg->ts_rate_decimator[i - 1] != 2 * cfg->ts_rate_decimator[i])
        ERROR("ts_rate_decimator factors are not powers of 2");
  }

  // VP9 does not support a lower bound on the keyframe interval in
  // automatic keyframe placement mode.
  if (cfg->kf_mode != VPX_KF_DISABLED &&
      cfg->kf_min_dist != cfg->kf_max_dist &&
      cfg->kf_min_dist > 0)
    ERROR("kf_min_dist not supported in auto mode, use 0 "
          "or kf_max_dist instead.");

  RANGE_CHECK_BOOL(extra_cfg,  enable_auto_alt_ref);
  RANGE_CHECK(extra_cfg, cpu_used, -16, 16);
  RANGE_CHECK_HI(extra_cfg, noise_sensitivity, 6);
  RANGE_CHECK(extra_cfg, tile_columns, 0, 6);
  RANGE_CHECK(extra_cfg, tile_rows, 0, 2);
  RANGE_CHECK_HI(extra_cfg, sharpness, 7);
  RANGE_CHECK(extra_cfg, arnr_max_frames, 0, 15);
  RANGE_CHECK_HI(extra_cfg, arnr_strength, 6);
  RANGE_CHECK(extra_cfg, arnr_type, 1, 3);
  RANGE_CHECK(extra_cfg, cq_level, 0, 63);

  // TODO(yaowu): remove this when ssim tuning is implemented for vp9
  if (extra_cfg->tuning == VP8_TUNE_SSIM)
      ERROR("Option --tune=ssim is not currently supported in VP9.");

  if (cfg->g_pass == VPX_RC_LAST_PASS) {
    size_t           packet_sz = sizeof(FIRSTPASS_STATS);
    int              n_packets = (int)(cfg->rc_twopass_stats_in.sz / packet_sz);
    const FIRSTPASS_STATS *stats;

    if (cfg->rc_twopass_stats_in.buf == NULL)
      ERROR("rc_twopass_stats_in.buf not set.");

    if (cfg->rc_twopass_stats_in.sz % packet_sz)
      ERROR("rc_twopass_stats_in.sz indicates truncated packet.");

    if (cfg->ss_number_layers > 1) {
      int i;
      unsigned int n_packets_per_layer[VPX_SS_MAX_LAYERS] = {0};

      stats = cfg->rc_twopass_stats_in.buf;
      for (i = 0; i < n_packets; ++i) {
        const int layer_id = (int)stats[i].spatial_layer_id;
        if (layer_id >= 0 && layer_id < (int)cfg->ss_number_layers) {
          ++n_packets_per_layer[layer_id];
        }
      }

      for (i = 0; i < (int)cfg->ss_number_layers; ++i) {
        unsigned int layer_id;
        if (n_packets_per_layer[i] < 2) {
          ERROR("rc_twopass_stats_in requires at least two packets for each "
                "layer.");
        }

        stats = (const FIRSTPASS_STATS *)cfg->rc_twopass_stats_in.buf +
                n_packets - cfg->ss_number_layers + i;
        layer_id = (int)stats->spatial_layer_id;

        if (layer_id >= cfg->ss_number_layers
            ||(unsigned int)(stats->count + 0.5) !=
               n_packets_per_layer[layer_id] - 1)
          ERROR("rc_twopass_stats_in missing EOS stats packet");
      }
    } else {
      if (cfg->rc_twopass_stats_in.sz < 2 * packet_sz)
        ERROR("rc_twopass_stats_in requires at least two packets.");

      stats =
          (const FIRSTPASS_STATS *)cfg->rc_twopass_stats_in.buf + n_packets - 1;

      if ((int)(stats->count + 0.5) != n_packets - 1)
        ERROR("rc_twopass_stats_in missing EOS stats packet");
    }
  }
  if (cfg->g_profile <= (unsigned int)PROFILE_1 &&
      extra_cfg->bit_depth > BITS_8)
    ERROR("High bit-depth not supported in profile < 2");
  if (cfg->g_profile > (unsigned int)PROFILE_1 &&
      extra_cfg->bit_depth == BITS_8)
    ERROR("Bit-depth 8 not supported in profile > 1");

  return VPX_CODEC_OK;
}


static vpx_codec_err_t validate_img(vpx_codec_alg_priv_t *ctx,
                                    const vpx_image_t *img) {
  switch (img->fmt) {
    case VPX_IMG_FMT_YV12:
    case VPX_IMG_FMT_I420:
    case VPX_IMG_FMT_I422:
    case VPX_IMG_FMT_I444:
      break;
    default:
      ERROR("Invalid image format. Only YV12, I420, I422, I444 images are "
            "supported.");
  }

  if (img->d_w != ctx->cfg.g_w || img->d_h != ctx->cfg.g_h)
    ERROR("Image size must match encoder init configuration size");

  return VPX_CODEC_OK;
}


static vpx_codec_err_t set_encoder_config(
    VP9EncoderConfig *oxcf,
    const vpx_codec_enc_cfg_t *cfg,
    const struct vp9_extracfg *extra_cfg) {
  oxcf->profile = cfg->g_profile;
  oxcf->width   = cfg->g_w;
  oxcf->height  = cfg->g_h;
  oxcf->bit_depth = extra_cfg->bit_depth;
  // guess a frame rate if out of whack, use 30
  oxcf->framerate = (double)cfg->g_timebase.den / cfg->g_timebase.num;
  if (oxcf->framerate > 180)
    oxcf->framerate = 30;

  switch (cfg->g_pass) {
    case VPX_RC_ONE_PASS:
      oxcf->mode = ONE_PASS_GOOD;
      break;
    case VPX_RC_FIRST_PASS:
      oxcf->mode = TWO_PASS_FIRST;
      break;
    case VPX_RC_LAST_PASS:
      oxcf->mode = TWO_PASS_SECOND_BEST;
      break;
  }

  oxcf->lag_in_frames = cfg->g_pass == VPX_RC_FIRST_PASS ? 0
                                                         : cfg->g_lag_in_frames;

  oxcf->rc_mode = RC_MODE_VBR;
  if (cfg->rc_end_usage == VPX_CQ)
    oxcf->rc_mode = RC_MODE_CONSTRAINED_QUALITY;
  else if (cfg->rc_end_usage == VPX_Q)
    oxcf->rc_mode = RC_MODE_CONSTANT_QUALITY;
  else if (cfg->rc_end_usage == VPX_CBR)
    oxcf->rc_mode = RC_MODE_CBR;

  oxcf->target_bandwidth         = cfg->rc_target_bitrate;
  oxcf->rc_max_intra_bitrate_pct = extra_cfg->rc_max_intra_bitrate_pct;

  oxcf->best_allowed_q  = vp9_quantizer_to_qindex(cfg->rc_min_quantizer);
  oxcf->worst_allowed_q = vp9_quantizer_to_qindex(cfg->rc_max_quantizer);
  oxcf->cq_level        = vp9_quantizer_to_qindex(extra_cfg->cq_level);
  oxcf->fixed_q = -1;

  oxcf->under_shoot_pct         = cfg->rc_undershoot_pct;
  oxcf->over_shoot_pct          = cfg->rc_overshoot_pct;

  oxcf->allow_spatial_resampling = cfg->rc_resize_allowed;
  oxcf->scaled_frame_width       = cfg->rc_scaled_width;
  oxcf->scaled_frame_height      = cfg->rc_scaled_height;

  oxcf->maximum_buffer_size     = cfg->rc_buf_sz;
  oxcf->starting_buffer_level   = cfg->rc_buf_initial_sz;
  oxcf->optimal_buffer_level    = cfg->rc_buf_optimal_sz;

  oxcf->drop_frames_water_mark   = cfg->rc_dropframe_thresh;

  oxcf->two_pass_vbrbias         = cfg->rc_2pass_vbr_bias_pct;
  oxcf->two_pass_vbrmin_section  = cfg->rc_2pass_vbr_minsection_pct;
  oxcf->two_pass_vbrmax_section  = cfg->rc_2pass_vbr_maxsection_pct;

  oxcf->auto_key               = cfg->kf_mode == VPX_KF_AUTO &&
                                 cfg->kf_min_dist != cfg->kf_max_dist;

  oxcf->key_freq               = cfg->kf_max_dist;

  oxcf->speed                  =  clamp(abs(extra_cfg->cpu_used), 0, 7);
  oxcf->encode_breakout        =  extra_cfg->static_thresh;
  oxcf->play_alternate         =  extra_cfg->enable_auto_alt_ref;
  oxcf->noise_sensitivity      =  extra_cfg->noise_sensitivity;
  oxcf->sharpness              =  extra_cfg->sharpness;

  oxcf->two_pass_stats_in      =  cfg->rc_twopass_stats_in;
  oxcf->output_pkt_list        =  extra_cfg->pkt_list;

  oxcf->arnr_max_frames = extra_cfg->arnr_max_frames;
  oxcf->arnr_strength   = extra_cfg->arnr_strength;
  oxcf->arnr_type       = extra_cfg->arnr_type;

  oxcf->tuning = extra_cfg->tuning;

  oxcf->tile_columns = extra_cfg->tile_columns;
  oxcf->tile_rows    = extra_cfg->tile_rows;

  oxcf->lossless = extra_cfg->lossless;

  oxcf->error_resilient_mode         = cfg->g_error_resilient;
  oxcf->frame_parallel_decoding_mode = extra_cfg->frame_parallel_decoding_mode;

  oxcf->aq_mode = extra_cfg->aq_mode;

  oxcf->frame_periodic_boost =  extra_cfg->frame_periodic_boost;

  oxcf->ss_number_layers = cfg->ss_number_layers;

  if (oxcf->ss_number_layers > 1) {
    vp9_copy(oxcf->ss_target_bitrate, cfg->ss_target_bitrate);
  } else if (oxcf->ss_number_layers == 1) {
    oxcf->ss_target_bitrate[0] = (int)oxcf->target_bandwidth;
  }

  oxcf->ts_number_layers = cfg->ts_number_layers;

  if (oxcf->ts_number_layers > 1) {
    vp9_copy(oxcf->ts_target_bitrate, cfg->ts_target_bitrate);
    vp9_copy(oxcf->ts_rate_decimator, cfg->ts_rate_decimator);
  } else if (oxcf->ts_number_layers == 1) {
    oxcf->ts_target_bitrate[0] = (int)oxcf->target_bandwidth;
    oxcf->ts_rate_decimator[0] = 1;
  }

  /*
  printf("Current VP9 Settings: \n");
  printf("target_bandwidth: %d\n", oxcf->target_bandwidth);
  printf("noise_sensitivity: %d\n", oxcf->noise_sensitivity);
  printf("sharpness: %d\n",    oxcf->sharpness);
  printf("cpu_used: %d\n",  oxcf->cpu_used);
  printf("Mode: %d\n",     oxcf->mode);
  printf("auto_key: %d\n",  oxcf->auto_key);
  printf("key_freq: %d\n", oxcf->key_freq);
  printf("end_usage: %d\n", oxcf->end_usage);
  printf("under_shoot_pct: %d\n", oxcf->under_shoot_pct);
  printf("over_shoot_pct: %d\n", oxcf->over_shoot_pct);
  printf("starting_buffer_level: %d\n", oxcf->starting_buffer_level);
  printf("optimal_buffer_level: %d\n",  oxcf->optimal_buffer_level);
  printf("maximum_buffer_size: %d\n", oxcf->maximum_buffer_size);
  printf("fixed_q: %d\n",  oxcf->fixed_q);
  printf("worst_allowed_q: %d\n", oxcf->worst_allowed_q);
  printf("best_allowed_q: %d\n", oxcf->best_allowed_q);
  printf("allow_spatial_resampling: %d\n", oxcf->allow_spatial_resampling);
  printf("scaled_frame_width: %d\n", oxcf->scaled_frame_width);
  printf("scaled_frame_height: %d\n", oxcf->scaled_frame_height);
  printf("two_pass_vbrbias: %d\n",  oxcf->two_pass_vbrbias);
  printf("two_pass_vbrmin_section: %d\n", oxcf->two_pass_vbrmin_section);
  printf("two_pass_vbrmax_section: %d\n", oxcf->two_pass_vbrmax_section);
  printf("lag_in_frames: %d\n", oxcf->lag_in_frames);
  printf("play_alternate: %d\n", oxcf->play_alternate);
  printf("Version: %d\n", oxcf->Version);
  printf("encode_breakout: %d\n", oxcf->encode_breakout);
  printf("error resilient: %d\n", oxcf->error_resilient_mode);
  printf("frame parallel detokenization: %d\n",
         oxcf->frame_parallel_decoding_mode);
  */
  return VPX_CODEC_OK;
}

static vpx_codec_err_t encoder_set_config(vpx_codec_alg_priv_t *ctx,
                                          const vpx_codec_enc_cfg_t  *cfg) {
  vpx_codec_err_t res;

  if (cfg->g_w != ctx->cfg.g_w || cfg->g_h != ctx->cfg.g_h)
    ERROR("Cannot change width or height after initialization");

  // Prevent increasing lag_in_frames. This check is stricter than it needs
  // to be -- the limit is not increasing past the first lag_in_frames
  // value, but we don't track the initial config, only the last successful
  // config.
  if (cfg->g_lag_in_frames > ctx->cfg.g_lag_in_frames)
    ERROR("Cannot increase lag_in_frames");

  res = validate_config(ctx, cfg, &ctx->extra_cfg);

  if (res == VPX_CODEC_OK) {
    ctx->cfg = *cfg;
    set_encoder_config(&ctx->oxcf, &ctx->cfg, &ctx->extra_cfg);
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }

  return res;
}

static vpx_codec_err_t ctrl_get_param(vpx_codec_alg_priv_t *ctx, int ctrl_id,
                                 va_list args) {
  void *arg = va_arg(args, void *);

#define MAP(id, var) case id: *(RECAST(id, arg)) = var; break

  if (arg == NULL)
    return VPX_CODEC_INVALID_PARAM;

  switch (ctrl_id) {
    MAP(VP8E_GET_LAST_QUANTIZER, vp9_get_quantizer(ctx->cpi));
    MAP(VP8E_GET_LAST_QUANTIZER_64,
        vp9_qindex_to_quantizer(vp9_get_quantizer(ctx->cpi)));
  }

  return VPX_CODEC_OK;
#undef MAP
}


static vpx_codec_err_t ctrl_set_param(vpx_codec_alg_priv_t *ctx, int ctrl_id,
                                      va_list args) {
  vpx_codec_err_t res = VPX_CODEC_OK;
  struct vp9_extracfg extra_cfg = ctx->extra_cfg;

#define MAP(id, var) case id: var = CAST(id, args); break;

  switch (ctrl_id) {
    MAP(VP8E_SET_CPUUSED,                 extra_cfg.cpu_used);
    MAP(VP8E_SET_ENABLEAUTOALTREF,        extra_cfg.enable_auto_alt_ref);
    MAP(VP8E_SET_NOISE_SENSITIVITY,       extra_cfg.noise_sensitivity);
    MAP(VP8E_SET_SHARPNESS,               extra_cfg.sharpness);
    MAP(VP8E_SET_STATIC_THRESHOLD,        extra_cfg.static_thresh);
    MAP(VP9E_SET_TILE_COLUMNS,            extra_cfg.tile_columns);
    MAP(VP9E_SET_TILE_ROWS,               extra_cfg.tile_rows);
    MAP(VP8E_SET_ARNR_MAXFRAMES,          extra_cfg.arnr_max_frames);
    MAP(VP8E_SET_ARNR_STRENGTH,           extra_cfg.arnr_strength);
    MAP(VP8E_SET_ARNR_TYPE,               extra_cfg.arnr_type);
    MAP(VP8E_SET_TUNING,                  extra_cfg.tuning);
    MAP(VP8E_SET_CQ_LEVEL,                extra_cfg.cq_level);
    MAP(VP8E_SET_MAX_INTRA_BITRATE_PCT,   extra_cfg.rc_max_intra_bitrate_pct);
    MAP(VP9E_SET_LOSSLESS,                extra_cfg.lossless);
    MAP(VP9E_SET_FRAME_PARALLEL_DECODING,
        extra_cfg.frame_parallel_decoding_mode);
    MAP(VP9E_SET_AQ_MODE,                 extra_cfg.aq_mode);
    MAP(VP9E_SET_FRAME_PERIODIC_BOOST,   extra_cfg.frame_periodic_boost);
  }

  res = validate_config(ctx, &ctx->cfg, &extra_cfg);

  if (res == VPX_CODEC_OK) {
    ctx->extra_cfg = extra_cfg;
    set_encoder_config(&ctx->oxcf, &ctx->cfg, &ctx->extra_cfg);
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }

  return res;
#undef MAP
}

static vpx_codec_err_t encoder_init(vpx_codec_ctx_t *ctx,
                                    vpx_codec_priv_enc_mr_cfg_t *data) {
  vpx_codec_err_t res = VPX_CODEC_OK;

  if (ctx->priv == NULL) {
    int i;
    vpx_codec_enc_cfg_t *cfg;
    struct vpx_codec_alg_priv *priv = calloc(1, sizeof(*priv));

    if (priv == NULL)
      return VPX_CODEC_MEM_ERROR;

    ctx->priv = &priv->base;
    ctx->priv->sz = sizeof(*ctx->priv);
    ctx->priv->iface = ctx->iface;
    ctx->priv->alg_priv = priv;
    ctx->priv->init_flags = ctx->init_flags;
    ctx->priv->enc.total_encoders = 1;

    if (ctx->config.enc) {
      // Update the reference to the config structure to an internal copy.
      ctx->priv->alg_priv->cfg = *ctx->config.enc;
      ctx->config.enc = &ctx->priv->alg_priv->cfg;
    }

    cfg = &ctx->priv->alg_priv->cfg;

    // Select the extra vp6 configuration table based on the current
    // usage value. If the current usage value isn't found, use the
    // values for usage case 0.
    for (i = 0;
         extracfg_map[i].usage && extracfg_map[i].usage != cfg->g_usage;
         ++i) {}

    priv->extra_cfg = extracfg_map[i].cfg;
    priv->extra_cfg.pkt_list = &priv->pkt_list.head;
     // Maximum buffer size approximated based on having multiple ARF.
    priv->cx_data_sz = priv->cfg.g_w * priv->cfg.g_h * 3 / 2 * 8;

    if (priv->cx_data_sz < 4096)
      priv->cx_data_sz = 4096;

    priv->cx_data = (unsigned char *)malloc(priv->cx_data_sz);
    if (priv->cx_data == NULL)
      return VPX_CODEC_MEM_ERROR;

    vp9_initialize_enc();

    res = validate_config(priv, &priv->cfg, &priv->extra_cfg);

    if (res == VPX_CODEC_OK) {
      VP9_COMP *cpi;
      set_encoder_config(&ctx->priv->alg_priv->oxcf,
                         &ctx->priv->alg_priv->cfg,
                         &ctx->priv->alg_priv->extra_cfg);
      cpi = vp9_create_compressor(&ctx->priv->alg_priv->oxcf);
      if (cpi == NULL)
        res = VPX_CODEC_MEM_ERROR;
      else
        ctx->priv->alg_priv->cpi = cpi;
    }
  }

  return res;
}

static vpx_codec_err_t encoder_destroy(vpx_codec_alg_priv_t *ctx) {
  free(ctx->cx_data);
  vp9_remove_compressor(ctx->cpi);
  free(ctx);
  return VPX_CODEC_OK;
}

static void pick_quickcompress_mode(vpx_codec_alg_priv_t  *ctx,
                                    unsigned long duration,
                                    unsigned long deadline) {
  // Use best quality mode if no deadline is given.
  MODE new_qc = ONE_PASS_BEST;

  if (deadline) {
    // Convert duration parameter from stream timebase to microseconds
    const uint64_t duration_us = (uint64_t)duration * 1000000 *
                               (uint64_t)ctx->cfg.g_timebase.num /
                               (uint64_t)ctx->cfg.g_timebase.den;

    // If the deadline is more that the duration this frame is to be shown,
    // use good quality mode. Otherwise use realtime mode.
    new_qc = (deadline > duration_us) ? ONE_PASS_GOOD : REALTIME;
  }

  if (ctx->cfg.g_pass == VPX_RC_FIRST_PASS)
    new_qc = TWO_PASS_FIRST;
  else if (ctx->cfg.g_pass == VPX_RC_LAST_PASS)
    new_qc = (new_qc == ONE_PASS_BEST) ? TWO_PASS_SECOND_BEST
                                          : TWO_PASS_SECOND_GOOD;

  if (ctx->oxcf.mode != new_qc) {
    ctx->oxcf.mode = new_qc;
    vp9_change_config(ctx->cpi, &ctx->oxcf);
  }
}

// Turn on to test if supplemental superframe data breaks decoding
// #define TEST_SUPPLEMENTAL_SUPERFRAME_DATA
static int write_superframe_index(vpx_codec_alg_priv_t *ctx) {
  uint8_t marker = 0xc0;
  unsigned int mask;
  int mag, index_sz;

  assert(ctx->pending_frame_count);
  assert(ctx->pending_frame_count <= 8);

  // Add the number of frames to the marker byte
  marker |= ctx->pending_frame_count - 1;

  // Choose the magnitude
  for (mag = 0, mask = 0xff; mag < 4; mag++) {
    if (ctx->pending_frame_magnitude < mask)
      break;
    mask <<= 8;
    mask |= 0xff;
  }
  marker |= mag << 3;

  // Write the index
  index_sz = 2 + (mag + 1) * ctx->pending_frame_count;
  if (ctx->pending_cx_data_sz + index_sz < ctx->cx_data_sz) {
    uint8_t *x = ctx->pending_cx_data + ctx->pending_cx_data_sz;
    int i, j;
#ifdef TEST_SUPPLEMENTAL_SUPERFRAME_DATA
    uint8_t marker_test = 0xc0;
    int mag_test = 2;     // 1 - 4
    int frames_test = 4;  // 1 - 8
    int index_sz_test = 2 + mag_test * frames_test;
    marker_test |= frames_test - 1;
    marker_test |= (mag_test - 1) << 3;
    *x++ = marker_test;
    for (i = 0; i < mag_test * frames_test; ++i)
      *x++ = 0;  // fill up with arbitrary data
    *x++ = marker_test;
    ctx->pending_cx_data_sz += index_sz_test;
    printf("Added supplemental superframe data\n");
#endif

    *x++ = marker;
    for (i = 0; i < ctx->pending_frame_count; i++) {
      unsigned int this_sz = (unsigned int)ctx->pending_frame_sizes[i];

      for (j = 0; j <= mag; j++) {
        *x++ = this_sz & 0xff;
        this_sz >>= 8;
      }
    }
    *x++ = marker;
    ctx->pending_cx_data_sz += index_sz;
#ifdef TEST_SUPPLEMENTAL_SUPERFRAME_DATA
    index_sz += index_sz_test;
#endif
  }
  return index_sz;
}

static vpx_codec_err_t encoder_encode(vpx_codec_alg_priv_t  *ctx,
                                      const vpx_image_t *img,
                                      vpx_codec_pts_t pts,
                                      unsigned long duration,
                                      vpx_enc_frame_flags_t flags,
                                      unsigned long deadline) {
  vpx_codec_err_t res = VPX_CODEC_OK;

  if (img)
    res = validate_img(ctx, img);

  pick_quickcompress_mode(ctx, duration, deadline);
  vpx_codec_pkt_list_init(&ctx->pkt_list);

  // Handle Flags
  if (((flags & VP8_EFLAG_NO_UPD_GF) && (flags & VP8_EFLAG_FORCE_GF)) ||
       ((flags & VP8_EFLAG_NO_UPD_ARF) && (flags & VP8_EFLAG_FORCE_ARF))) {
    ctx->base.err_detail = "Conflicting flags.";
    return VPX_CODEC_INVALID_PARAM;
  }

  if (flags & (VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_GF |
               VP8_EFLAG_NO_REF_ARF)) {
    int ref = 7;

    if (flags & VP8_EFLAG_NO_REF_LAST)
      ref ^= VP9_LAST_FLAG;

    if (flags & VP8_EFLAG_NO_REF_GF)
      ref ^= VP9_GOLD_FLAG;

    if (flags & VP8_EFLAG_NO_REF_ARF)
      ref ^= VP9_ALT_FLAG;

    vp9_use_as_reference(ctx->cpi, ref);
  }

  if (flags & (VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF |
               VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_FORCE_GF |
               VP8_EFLAG_FORCE_ARF)) {
    int upd = 7;

    if (flags & VP8_EFLAG_NO_UPD_LAST)
      upd ^= VP9_LAST_FLAG;

    if (flags & VP8_EFLAG_NO_UPD_GF)
      upd ^= VP9_GOLD_FLAG;

    if (flags & VP8_EFLAG_NO_UPD_ARF)
      upd ^= VP9_ALT_FLAG;

    vp9_update_reference(ctx->cpi, upd);
  }

  if (flags & VP8_EFLAG_NO_UPD_ENTROPY) {
    vp9_update_entropy(ctx->cpi, 0);
  }

  // Handle fixed keyframe intervals
  if (ctx->cfg.kf_mode == VPX_KF_AUTO &&
      ctx->cfg.kf_min_dist == ctx->cfg.kf_max_dist) {
    if (++ctx->fixed_kf_cntr > ctx->cfg.kf_min_dist) {
      flags |= VPX_EFLAG_FORCE_KF;
      ctx->fixed_kf_cntr = 1;
    }
  }

  // Initialize the encoder instance on the first frame.
  if (res == VPX_CODEC_OK && ctx->cpi != NULL) {
    unsigned int lib_flags;
    YV12_BUFFER_CONFIG sd;
    int64_t dst_time_stamp, dst_end_time_stamp;
    size_t size, cx_data_sz;
    unsigned char *cx_data;

    // Set up internal flags
    if (ctx->base.init_flags & VPX_CODEC_USE_PSNR)
      ((VP9_COMP *)ctx->cpi)->b_calculate_psnr = 1;

    // Convert API flags to internal codec lib flags
    lib_flags = (flags & VPX_EFLAG_FORCE_KF) ? FRAMEFLAGS_KEY : 0;

    /* vp9 use 10,000,000 ticks/second as time stamp */
    dst_time_stamp = (pts * 10000000 * ctx->cfg.g_timebase.num)
                     / ctx->cfg.g_timebase.den;
    dst_end_time_stamp = (pts + duration) * 10000000 * ctx->cfg.g_timebase.num /
                         ctx->cfg.g_timebase.den;

    if (img != NULL) {
      res = image2yuvconfig(img, &sd);

      if (vp9_receive_raw_frame(ctx->cpi, lib_flags,
                                &sd, dst_time_stamp, dst_end_time_stamp)) {
        VP9_COMP *cpi = (VP9_COMP *)ctx->cpi;
        res = update_error_state(ctx, &cpi->common.error);
      }
    }

    cx_data = ctx->cx_data;
    cx_data_sz = ctx->cx_data_sz;
    lib_flags = 0;

    /* Any pending invisible frames? */
    if (ctx->pending_cx_data) {
      memmove(cx_data, ctx->pending_cx_data, ctx->pending_cx_data_sz);
      ctx->pending_cx_data = cx_data;
      cx_data += ctx->pending_cx_data_sz;
      cx_data_sz -= ctx->pending_cx_data_sz;

      /* TODO: this is a minimal check, the underlying codec doesn't respect
       * the buffer size anyway.
       */
      if (cx_data_sz < ctx->cx_data_sz / 2) {
        ctx->base.err_detail = "Compressed data buffer too small";
        return VPX_CODEC_ERROR;
      }
    }

    while (cx_data_sz >= ctx->cx_data_sz / 2 &&
           -1 != vp9_get_compressed_data(ctx->cpi, &lib_flags, &size,
                                         cx_data, &dst_time_stamp,
                                         &dst_end_time_stamp, !img)) {
      if (size) {
        vpx_codec_pts_t round, delta;
        vpx_codec_cx_pkt_t pkt;
        VP9_COMP *const cpi = (VP9_COMP *)ctx->cpi;

        // Pack invisible frames with the next visible frame
        if (cpi->common.show_frame == 0) {
          if (ctx->pending_cx_data == 0)
            ctx->pending_cx_data = cx_data;
          ctx->pending_cx_data_sz += size;
          ctx->pending_frame_sizes[ctx->pending_frame_count++] = size;
          ctx->pending_frame_magnitude |= size;
          cx_data += size;
          cx_data_sz -= size;
          continue;
        }

        // Add the frame packet to the list of returned packets.
        round = (vpx_codec_pts_t)10000000 * ctx->cfg.g_timebase.num / 2 - 1;
        delta = (dst_end_time_stamp - dst_time_stamp);
        pkt.kind = VPX_CODEC_CX_FRAME_PKT;
        pkt.data.frame.pts =
          (dst_time_stamp * ctx->cfg.g_timebase.den + round)
          / ctx->cfg.g_timebase.num / 10000000;
        pkt.data.frame.duration = (unsigned long)
          ((delta * ctx->cfg.g_timebase.den + round)
          / ctx->cfg.g_timebase.num / 10000000);
        pkt.data.frame.flags = lib_flags << 16;

        if (lib_flags & FRAMEFLAGS_KEY)
          pkt.data.frame.flags |= VPX_FRAME_IS_KEY;

        if (cpi->common.show_frame == 0) {
          pkt.data.frame.flags |= VPX_FRAME_IS_INVISIBLE;

          // This timestamp should be as close as possible to the
          // prior PTS so that if a decoder uses pts to schedule when
          // to do this, we start right after last frame was decoded.
          // Invisible frames have no duration.
          pkt.data.frame.pts = ((cpi->last_time_stamp_seen
                                 * ctx->cfg.g_timebase.den + round)
                                / ctx->cfg.g_timebase.num / 10000000) + 1;
          pkt.data.frame.duration = 0;
        }

        if (cpi->droppable)
          pkt.data.frame.flags |= VPX_FRAME_IS_DROPPABLE;

        if (ctx->pending_cx_data) {
          ctx->pending_frame_sizes[ctx->pending_frame_count++] = size;
          ctx->pending_frame_magnitude |= size;
          ctx->pending_cx_data_sz += size;
          size += write_superframe_index(ctx);
          pkt.data.frame.buf = ctx->pending_cx_data;
          pkt.data.frame.sz  = ctx->pending_cx_data_sz;
          ctx->pending_cx_data = NULL;
          ctx->pending_cx_data_sz = 0;
          ctx->pending_frame_count = 0;
          ctx->pending_frame_magnitude = 0;
        } else {
          pkt.data.frame.buf = cx_data;
          pkt.data.frame.sz  = size;
        }
        pkt.data.frame.partition_id = -1;
        vpx_codec_pkt_list_add(&ctx->pkt_list.head, &pkt);
        cx_data += size;
        cx_data_sz -= size;
      }
    }
  }

  return res;
}

static const vpx_codec_cx_pkt_t *encoder_get_cxdata(vpx_codec_alg_priv_t  *ctx,
                                                    vpx_codec_iter_t *iter) {
  return vpx_codec_pkt_list_get(&ctx->pkt_list.head, iter);
}

static vpx_codec_err_t ctrl_set_reference(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id, va_list args) {
  vpx_ref_frame_t *const frame = va_arg(args, vpx_ref_frame_t *);

  if (frame != NULL) {
    YV12_BUFFER_CONFIG sd;

    image2yuvconfig(&frame->img, &sd);
    vp9_set_reference_enc(ctx->cpi, ref_frame_to_vp9_reframe(frame->frame_type),
                          &sd);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_copy_reference(vpx_codec_alg_priv_t *ctx,
                                           int ctr_id, va_list args) {
  vpx_ref_frame_t *const frame = va_arg(args, vpx_ref_frame_t *);

  if (frame != NULL) {
    YV12_BUFFER_CONFIG sd;

    image2yuvconfig(&frame->img, &sd);
    vp9_copy_reference_enc(ctx->cpi,
                           ref_frame_to_vp9_reframe(frame->frame_type), &sd);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_get_reference(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id, va_list args) {
  vp9_ref_frame_t *frame = va_arg(args, vp9_ref_frame_t *);

  if (frame != NULL) {
    YV12_BUFFER_CONFIG* fb;

    vp9_get_reference_enc(ctx->cpi, frame->idx, &fb);
    yuvconfig2image(&frame->img, fb, NULL);
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_set_previewpp(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id, va_list args) {
#if CONFIG_VP9_POSTPROC
  vp8_postproc_cfg_t *config = va_arg(args, vp8_postproc_cfg_t *);
  (void)ctr_id;

  if (config != NULL) {
    ctx->preview_ppcfg = *config;
    return VPX_CODEC_OK;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
#else
  (void)ctx;
  (void)ctr_id;
  (void)args;
  return VPX_CODEC_INCAPABLE;
#endif
}


static vpx_image_t *encoder_get_preview(vpx_codec_alg_priv_t *ctx) {
  YV12_BUFFER_CONFIG sd;
  vp9_ppflags_t flags = {0};

  if (ctx->preview_ppcfg.post_proc_flag) {
    flags.post_proc_flag   = ctx->preview_ppcfg.post_proc_flag;
    flags.deblocking_level = ctx->preview_ppcfg.deblocking_level;
    flags.noise_level      = ctx->preview_ppcfg.noise_level;
  }

  if (vp9_get_preview_raw_frame(ctx->cpi, &sd, &flags) == 0) {
    yuvconfig2image(&ctx->preview_img, &sd, NULL);
    return &ctx->preview_img;
  } else {
    return NULL;
  }
}

static vpx_codec_err_t ctrl_update_entropy(vpx_codec_alg_priv_t *ctx,
                                           int ctr_id, va_list args) {
  const int update = va_arg(args, int);
  vp9_update_entropy(ctx->cpi, update);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_update_reference(vpx_codec_alg_priv_t *ctx,
                                             int ctr_id, va_list args) {
  const int ref_frame_flags = va_arg(args, int);
  vp9_update_reference(ctx->cpi, ref_frame_flags);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_use_reference(vpx_codec_alg_priv_t *ctx,
                                          int ctr_id, va_list args) {
  const int reference_flag = va_arg(args, int);
  vp9_use_as_reference(ctx->cpi, reference_flag);
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_roi_map(vpx_codec_alg_priv_t *ctx,
                                        int ctr_id, va_list args) {
  // TODO(yaowu): Need to re-implement and test for VP9.
  return VPX_CODEC_INVALID_PARAM;
}


static vpx_codec_err_t ctrl_set_active_map(vpx_codec_alg_priv_t *ctx,
                                           int ctr_id, va_list args) {
  vpx_active_map_t *const map = va_arg(args, vpx_active_map_t *);

  if (map) {
    if (!vp9_set_active_map(ctx->cpi, map->active_map,
                            (int)map->rows, (int)map->cols))
      return VPX_CODEC_OK;
    else
      return VPX_CODEC_INVALID_PARAM;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_set_scale_mode(vpx_codec_alg_priv_t *ctx,
                                           int ctr_id, va_list args) {
  vpx_scaling_mode_t *const mode = va_arg(args, vpx_scaling_mode_t *);

  if (mode) {
    const int res = vp9_set_internal_size(ctx->cpi,
                                          (VPX_SCALING)mode->h_scaling_mode,
                                          (VPX_SCALING)mode->v_scaling_mode);
    return (res == 0) ? VPX_CODEC_OK : VPX_CODEC_INVALID_PARAM;
  } else {
    return VPX_CODEC_INVALID_PARAM;
  }
}

static vpx_codec_err_t ctrl_set_svc(vpx_codec_alg_priv_t *ctx, int ctr_id,
                                    va_list args) {
  int data = va_arg(args, int);
  const vpx_codec_enc_cfg_t *cfg = &ctx->cfg;
  vp9_set_svc(ctx->cpi, data);
  // CBR or two pass mode for SVC with both temporal and spatial layers
  // not yet supported.
  if (data == 1 &&
      (cfg->rc_end_usage == VPX_CBR ||
       cfg->g_pass == VPX_RC_FIRST_PASS ||
       cfg->g_pass == VPX_RC_LAST_PASS) &&
      cfg->ss_number_layers > 1 &&
      cfg->ts_number_layers > 1) {
    return VPX_CODEC_INVALID_PARAM;
  }
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_svc_layer_id(vpx_codec_alg_priv_t *ctx,
                                             int ctr_id,
                                             va_list args) {
  vpx_svc_layer_id_t *const data = va_arg(args, vpx_svc_layer_id_t *);
  VP9_COMP *const cpi = (VP9_COMP *)ctx->cpi;
  SVC *const svc = &cpi->svc;
  svc->spatial_layer_id = data->spatial_layer_id;
  svc->temporal_layer_id = data->temporal_layer_id;
  // Checks on valid layer_id input.
  if (svc->temporal_layer_id < 0 ||
      svc->temporal_layer_id >= (int)ctx->cfg.ts_number_layers) {
    return VPX_CODEC_INVALID_PARAM;
  }
  if (svc->spatial_layer_id < 0 ||
      svc->spatial_layer_id >= (int)ctx->cfg.ss_number_layers) {
    return VPX_CODEC_INVALID_PARAM;
  }
  return VPX_CODEC_OK;
}

static vpx_codec_err_t ctrl_set_svc_parameters(vpx_codec_alg_priv_t *ctx,
                                               int ctr_id, va_list args) {
  VP9_COMP *const cpi = ctx->cpi;
  vpx_svc_parameters_t *const params = va_arg(args, vpx_svc_parameters_t *);

  if (params == NULL)
    return VPX_CODEC_INVALID_PARAM;

  cpi->svc.spatial_layer_id = params->spatial_layer;
  cpi->svc.temporal_layer_id = params->temporal_layer;

  cpi->lst_fb_idx = params->lst_fb_idx;
  cpi->gld_fb_idx = params->gld_fb_idx;
  cpi->alt_fb_idx = params->alt_fb_idx;

  if (vp9_set_size_literal(ctx->cpi, params->width, params->height) != 0)
    return VPX_CODEC_INVALID_PARAM;

  ctx->cfg.rc_max_quantizer = params->max_quantizer;
  ctx->cfg.rc_min_quantizer = params->min_quantizer;

  set_encoder_config(&ctx->oxcf, &ctx->cfg, &ctx->extra_cfg);
  vp9_change_config(ctx->cpi, &ctx->oxcf);

  return VPX_CODEC_OK;
}

static vpx_codec_ctrl_fn_map_t encoder_ctrl_maps[] = {
  {VP8_COPY_REFERENCE,                ctrl_copy_reference},
  {VP8E_UPD_ENTROPY,                  ctrl_update_entropy},
  {VP8E_UPD_REFERENCE,                ctrl_update_reference},
  {VP8E_USE_REFERENCE,                ctrl_use_reference},

  // Setters
  {VP8_SET_REFERENCE,                 ctrl_set_reference},
  {VP8_SET_POSTPROC,                  ctrl_set_previewpp},
  {VP8E_SET_ROI_MAP,                  ctrl_set_roi_map},
  {VP8E_SET_ACTIVEMAP,                ctrl_set_active_map},
  {VP8E_SET_SCALEMODE,                ctrl_set_scale_mode},
  {VP8E_SET_CPUUSED,                  ctrl_set_param},
  {VP8E_SET_NOISE_SENSITIVITY,        ctrl_set_param},
  {VP8E_SET_ENABLEAUTOALTREF,         ctrl_set_param},
  {VP8E_SET_SHARPNESS,                ctrl_set_param},
  {VP8E_SET_STATIC_THRESHOLD,         ctrl_set_param},
  {VP9E_SET_TILE_COLUMNS,             ctrl_set_param},
  {VP9E_SET_TILE_ROWS,                ctrl_set_param},
  {VP8E_SET_ARNR_MAXFRAMES,           ctrl_set_param},
  {VP8E_SET_ARNR_STRENGTH,            ctrl_set_param},
  {VP8E_SET_ARNR_TYPE,                ctrl_set_param},
  {VP8E_SET_TUNING,                   ctrl_set_param},
  {VP8E_SET_CQ_LEVEL,                 ctrl_set_param},
  {VP8E_SET_MAX_INTRA_BITRATE_PCT,    ctrl_set_param},
  {VP9E_SET_LOSSLESS,                 ctrl_set_param},
  {VP9E_SET_FRAME_PARALLEL_DECODING,  ctrl_set_param},
  {VP9E_SET_AQ_MODE,                  ctrl_set_param},
  {VP9E_SET_FRAME_PERIODIC_BOOST,     ctrl_set_param},
  {VP9E_SET_SVC,                      ctrl_set_svc},
  {VP9E_SET_SVC_PARAMETERS,           ctrl_set_svc_parameters},
  {VP9E_SET_SVC_LAYER_ID,             ctrl_set_svc_layer_id},

  // Getters
  {VP8E_GET_LAST_QUANTIZER,           ctrl_get_param},
  {VP8E_GET_LAST_QUANTIZER_64,        ctrl_get_param},
  {VP9_GET_REFERENCE,                 ctrl_get_reference},

  { -1, NULL},
};

static vpx_codec_enc_cfg_map_t encoder_usage_cfg_map[] = {
  {
    0,
    {  // NOLINT
      0,                  // g_usage
      0,                  // g_threads
      0,                  // g_profile

      320,                // g_width
      240,                // g_height
      {1, 30},            // g_timebase

      0,                  // g_error_resilient

      VPX_RC_ONE_PASS,    // g_pass

      25,                 // g_lag_in_frames

      0,                  // rc_dropframe_thresh
      0,                  // rc_resize_allowed
      1,                  // rc_scaled_width
      1,                  // rc_scaled_height
      60,                 // rc_resize_down_thresold
      30,                 // rc_resize_up_thresold

      VPX_VBR,            // rc_end_usage
#if VPX_ENCODER_ABI_VERSION > (1 + VPX_CODEC_ABI_VERSION)
      {0},                // rc_twopass_stats_in
#endif
      256,                // rc_target_bandwidth
      0,                  // rc_min_quantizer
      63,                 // rc_max_quantizer
      100,                // rc_undershoot_pct
      100,                // rc_overshoot_pct

      6000,               // rc_max_buffer_size
      4000,               // rc_buffer_initial_size
      5000,               // rc_buffer_optimal_size

      50,                 // rc_two_pass_vbrbias
      0,                  // rc_two_pass_vbrmin_section
      2000,               // rc_two_pass_vbrmax_section

      // keyframing settings (kf)
      VPX_KF_AUTO,        // g_kfmode
      0,                  // kf_min_dist
      9999,               // kf_max_dist

      VPX_SS_DEFAULT_LAYERS,  // ss_number_layers
      {0},                    // ss_target_bitrate
      1,                      // ts_number_layers
      {0},                    // ts_target_bitrate
      {0},                    // ts_rate_decimator
      0,                      // ts_periodicity
      {0},                    // ts_layer_id
#if VPX_ENCODER_ABI_VERSION == (1 + VPX_CODEC_ABI_VERSION)
      "vp8.fpf"           // first pass filename
#endif
    }
  },
  { -1, {NOT_IMPLEMENTED}}
};

#ifndef VERSION_STRING
#define VERSION_STRING
#endif
CODEC_INTERFACE(vpx_codec_vp9_cx) = {
  "WebM Project VP9 Encoder" VERSION_STRING,
  VPX_CODEC_INTERNAL_ABI_VERSION,
  VPX_CODEC_CAP_ENCODER | VPX_CODEC_CAP_PSNR,  // vpx_codec_caps_t
  encoder_init,       // vpx_codec_init_fn_t
  encoder_destroy,    // vpx_codec_destroy_fn_t
  encoder_ctrl_maps,  // vpx_codec_ctrl_fn_map_t
  NOT_IMPLEMENTED,    // vpx_codec_get_mmap_fn_t
  NOT_IMPLEMENTED,    // vpx_codec_set_mmap_fn_t
  {  // NOLINT
    NOT_IMPLEMENTED,  // vpx_codec_peek_si_fn_t
    NOT_IMPLEMENTED,  // vpx_codec_get_si_fn_t
    NOT_IMPLEMENTED,  // vpx_codec_decode_fn_t
    NOT_IMPLEMENTED,  // vpx_codec_frame_get_fn_t
  },
  {  // NOLINT
    encoder_usage_cfg_map,  // vpx_codec_enc_cfg_map_t
    encoder_encode,         // vpx_codec_encode_fn_t
    encoder_get_cxdata,     // vpx_codec_get_cx_data_fn_t
    encoder_set_config,     // vpx_codec_enc_config_set_fn_t
    NOT_IMPLEMENTED,        // vpx_codec_get_global_headers_fn_t
    encoder_get_preview,    // vpx_codec_get_preview_frame_fn_t
    NOT_IMPLEMENTED ,       // vpx_codec_enc_mr_get_mem_loc_fn_t
  }
};
