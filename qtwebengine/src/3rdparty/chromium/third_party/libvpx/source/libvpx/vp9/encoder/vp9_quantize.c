/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "vpx_mem/vpx_mem.h"

#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_seg_common.h"

#include "vp9/encoder/vp9_encoder.h"
#include "vp9/encoder/vp9_quantize.h"
#include "vp9/encoder/vp9_rdopt.h"

void vp9_quantize_b_c(const int16_t *coeff_ptr, intptr_t count,
                      int skip_block,
                      const int16_t *zbin_ptr, const int16_t *round_ptr,
                      const int16_t *quant_ptr, const int16_t *quant_shift_ptr,
                      int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                      const int16_t *dequant_ptr,
                      int zbin_oq_value, uint16_t *eob_ptr,
                      const int16_t *scan, const int16_t *iscan) {
  int i, non_zero_count = (int)count, eob = -1;
  const int zbins[2] = { zbin_ptr[0] + zbin_oq_value,
                         zbin_ptr[1] + zbin_oq_value };
  const int nzbins[2] = { zbins[0] * -1,
                          zbins[1] * -1 };
  (void)iscan;

  vpx_memset(qcoeff_ptr, 0, count * sizeof(int16_t));
  vpx_memset(dqcoeff_ptr, 0, count * sizeof(int16_t));

  if (!skip_block) {
    // Pre-scan pass
    for (i = (int)count - 1; i >= 0; i--) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];

      if (coeff < zbins[rc != 0] && coeff > nzbins[rc != 0])
        non_zero_count--;
      else
        break;
    }

    // Quantization pass: All coefficients with index >= zero_flag are
    // skippable. Note: zero_flag can be zero.
    for (i = 0; i < non_zero_count; i++) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];
      const int coeff_sign = (coeff >> 31);
      const int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;

      if (abs_coeff >= zbins[rc != 0]) {
        int tmp = clamp(abs_coeff + round_ptr[rc != 0], INT16_MIN, INT16_MAX);
        tmp = ((((tmp * quant_ptr[rc != 0]) >> 16) + tmp) *
                  quant_shift_ptr[rc != 0]) >> 16;  // quantization
        qcoeff_ptr[rc]  = (tmp ^ coeff_sign) - coeff_sign;
        dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant_ptr[rc != 0];

        if (tmp)
          eob = i;
      }
    }
  }
  *eob_ptr = eob + 1;
}

void vp9_quantize_b_32x32_c(const int16_t *coeff_ptr, intptr_t n_coeffs,
                            int skip_block,
                            const int16_t *zbin_ptr, const int16_t *round_ptr,
                            const int16_t *quant_ptr,
                            const int16_t *quant_shift_ptr,
                            int16_t *qcoeff_ptr, int16_t *dqcoeff_ptr,
                            const int16_t *dequant_ptr,
                            int zbin_oq_value, uint16_t *eob_ptr,
                            const int16_t *scan, const int16_t *iscan) {
  const int zbins[2] = { ROUND_POWER_OF_TWO(zbin_ptr[0] + zbin_oq_value, 1),
                         ROUND_POWER_OF_TWO(zbin_ptr[1] + zbin_oq_value, 1) };
  const int nzbins[2] = {zbins[0] * -1, zbins[1] * -1};

  int idx = 0;
  int idx_arr[1024];
  int i, eob = -1;
  (void)iscan;

  vpx_memset(qcoeff_ptr, 0, n_coeffs * sizeof(int16_t));
  vpx_memset(dqcoeff_ptr, 0, n_coeffs * sizeof(int16_t));

  if (!skip_block) {
    // Pre-scan pass
    for (i = 0; i < n_coeffs; i++) {
      const int rc = scan[i];
      const int coeff = coeff_ptr[rc];

      // If the coefficient is out of the base ZBIN range, keep it for
      // quantization.
      if (coeff >= zbins[rc != 0] || coeff <= nzbins[rc != 0])
        idx_arr[idx++] = i;
    }

    // Quantization pass: only process the coefficients selected in
    // pre-scan pass. Note: idx can be zero.
    for (i = 0; i < idx; i++) {
      const int rc = scan[idx_arr[i]];
      const int coeff = coeff_ptr[rc];
      const int coeff_sign = (coeff >> 31);
      int tmp;
      int abs_coeff = (coeff ^ coeff_sign) - coeff_sign;
      abs_coeff += ROUND_POWER_OF_TWO(round_ptr[rc != 0], 1);
      abs_coeff = clamp(abs_coeff, INT16_MIN, INT16_MAX);
      tmp = ((((abs_coeff * quant_ptr[rc != 0]) >> 16) + abs_coeff) *
               quant_shift_ptr[rc != 0]) >> 15;

      qcoeff_ptr[rc] = (tmp ^ coeff_sign) - coeff_sign;
      dqcoeff_ptr[rc] = qcoeff_ptr[rc] * dequant_ptr[rc != 0] / 2;

      if (tmp)
        eob = idx_arr[i];
    }
  }
  *eob_ptr = eob + 1;
}

void vp9_regular_quantize_b_4x4(MACROBLOCK *x, int plane, int block,
                                const int16_t *scan, const int16_t *iscan) {
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];

  vp9_quantize_b(BLOCK_OFFSET(p->coeff, block),
           16, x->skip_block,
           p->zbin, p->round, p->quant, p->quant_shift,
           BLOCK_OFFSET(p->qcoeff, block),
           BLOCK_OFFSET(pd->dqcoeff, block),
           pd->dequant, p->zbin_extra, &p->eobs[block], scan, iscan);
}

static void invert_quant(int16_t *quant, int16_t *shift, int d) {
  unsigned t;
  int l;
  t = d;
  for (l = 0; t > 1; l++)
    t >>= 1;
  t = 1 + (1 << (16 + l)) / d;
  *quant = (int16_t)(t - (1 << 16));
  *shift = 1 << (16 - l);
}

void vp9_init_quantizer(VP9_COMP *cpi) {
  VP9_COMMON *const cm = &cpi->common;
  QUANTS *const quants = &cpi->quants;
  int i, q, quant;

  for (q = 0; q < QINDEX_RANGE; q++) {
    const int qzbin_factor = q == 0 ? 64 : (vp9_dc_quant(q, 0) < 148 ? 84 : 80);
    const int qrounding_factor = q == 0 ? 64 : 48;

    for (i = 0; i < 2; ++i) {
      // y
      quant = i == 0 ? vp9_dc_quant(q, cm->y_dc_delta_q)
                     : vp9_ac_quant(q, 0);
      invert_quant(&quants->y_quant[q][i], &quants->y_quant_shift[q][i], quant);
      quants->y_zbin[q][i] = ROUND_POWER_OF_TWO(qzbin_factor * quant, 7);
      quants->y_round[q][i] = (qrounding_factor * quant) >> 7;
      cm->y_dequant[q][i] = quant;

      // uv
      quant = i == 0 ? vp9_dc_quant(q, cm->uv_dc_delta_q)
                     : vp9_ac_quant(q, cm->uv_ac_delta_q);
      invert_quant(&quants->uv_quant[q][i],
                   &quants->uv_quant_shift[q][i], quant);
      quants->uv_zbin[q][i] = ROUND_POWER_OF_TWO(qzbin_factor * quant, 7);
      quants->uv_round[q][i] = (qrounding_factor * quant) >> 7;
      cm->uv_dequant[q][i] = quant;

#if CONFIG_ALPHA
      // alpha
      quant = i == 0 ? vp9_dc_quant(q, cm->a_dc_delta_q)
                     : vp9_ac_quant(q, cm->a_ac_delta_q);
      invert_quant(&quants->a_quant[q][i], &quants->a_quant_shift[q][i], quant);
      quants->a_zbin[q][i] = ROUND_POWER_OF_TWO(qzbin_factor * quant, 7);
      quants->a_round[q][i] = (qrounding_factor * quant) >> 7;
      cm->a_dequant[q][i] = quant;
#endif
    }

    for (i = 2; i < 8; i++) {
      quants->y_quant[q][i] = quants->y_quant[q][1];
      quants->y_quant_shift[q][i] = quants->y_quant_shift[q][1];
      quants->y_zbin[q][i] = quants->y_zbin[q][1];
      quants->y_round[q][i] = quants->y_round[q][1];
      cm->y_dequant[q][i] = cm->y_dequant[q][1];

      quants->uv_quant[q][i] = quants->uv_quant[q][1];
      quants->uv_quant_shift[q][i] = quants->uv_quant_shift[q][1];
      quants->uv_zbin[q][i] = quants->uv_zbin[q][1];
      quants->uv_round[q][i] = quants->uv_round[q][1];
      cm->uv_dequant[q][i] = cm->uv_dequant[q][1];

#if CONFIG_ALPHA
      quants->a_quant[q][i] = quants->a_quant[q][1];
      quants->a_quant_shift[q][i] = quants->a_quant_shift[q][1];
      quants->a_zbin[q][i] = quants->a_zbin[q][1];
      quants->a_round[q][i] = quants->a_round[q][1];
      cm->a_dequant[q][i] = cm->a_dequant[q][1];
#endif
    }
  }
}

void vp9_init_plane_quantizers(VP9_COMP *cpi, MACROBLOCK *x) {
  const VP9_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  QUANTS *const quants = &cpi->quants;
  const int segment_id = xd->mi[0]->mbmi.segment_id;
  const int qindex = vp9_get_qindex(&cm->seg, segment_id, cm->base_qindex);
  const int rdmult = vp9_compute_rd_mult(cpi, qindex + cm->y_dc_delta_q);
  const int zbin = cpi->zbin_mode_boost;
  int i;

  // Y
  x->plane[0].quant = quants->y_quant[qindex];
  x->plane[0].quant_shift = quants->y_quant_shift[qindex];
  x->plane[0].zbin = quants->y_zbin[qindex];
  x->plane[0].round = quants->y_round[qindex];
  x->plane[0].zbin_extra = (int16_t)((cm->y_dequant[qindex][1] * zbin) >> 7);
  xd->plane[0].dequant = cm->y_dequant[qindex];

  // UV
  for (i = 1; i < 3; i++) {
    x->plane[i].quant = quants->uv_quant[qindex];
    x->plane[i].quant_shift = quants->uv_quant_shift[qindex];
    x->plane[i].zbin = quants->uv_zbin[qindex];
    x->plane[i].round = quants->uv_round[qindex];
    x->plane[i].zbin_extra = (int16_t)((cm->uv_dequant[qindex][1] * zbin) >> 7);
    xd->plane[i].dequant = cm->uv_dequant[qindex];
  }

#if CONFIG_ALPHA
  x->plane[3].quant = quants->a_quant[qindex];
  x->plane[3].quant_shift = quants->a_quant_shift[qindex];
  x->plane[3].zbin = quants->a_zbin[qindex];
  x->plane[3].round = quants->a_round[qindex];
  x->plane[3].zbin_extra = (int16_t)((cm->a_dequant[qindex][1] * zbin) >> 7);
  xd->plane[3].dequant = cm->a_dequant[qindex];
#endif

  x->skip_block = vp9_segfeature_active(&cm->seg, segment_id, SEG_LVL_SKIP);
  x->q_index = qindex;

  x->errorperbit = rdmult >> 6;
  x->errorperbit += (x->errorperbit == 0);

  vp9_initialize_me_consts(cpi, x->q_index);
}

void vp9_update_zbin_extra(VP9_COMP *cpi, MACROBLOCK *x) {
  const int qindex = x->q_index;
  const int y_zbin_extra = (cpi->common.y_dequant[qindex][1] *
                            cpi->zbin_mode_boost) >> 7;
  const int uv_zbin_extra = (cpi->common.uv_dequant[qindex][1] *
                             cpi->zbin_mode_boost) >> 7;

  x->plane[0].zbin_extra = (int16_t)y_zbin_extra;
  x->plane[1].zbin_extra = (int16_t)uv_zbin_extra;
  x->plane[2].zbin_extra = (int16_t)uv_zbin_extra;
}

void vp9_frame_init_quantizer(VP9_COMP *cpi) {
  cpi->zbin_mode_boost = 0;
  vp9_init_plane_quantizers(cpi, &cpi->mb);
}

void vp9_set_quantizer(VP9_COMMON *cm, int q) {
  // quantizer has to be reinitialized with vp9_init_quantizer() if any
  // delta_q changes.
  cm->base_qindex = q;
  cm->y_dc_delta_q = 0;
  cm->uv_dc_delta_q = 0;
  cm->uv_ac_delta_q = 0;
}

// Table that converts 0-63 Q-range values passed in outside to the Qindex
// range used internally.
static const int quantizer_to_qindex[] = {
  0,    4,   8,  12,  16,  20,  24,  28,
  32,   36,  40,  44,  48,  52,  56,  60,
  64,   68,  72,  76,  80,  84,  88,  92,
  96,  100, 104, 108, 112, 116, 120, 124,
  128, 132, 136, 140, 144, 148, 152, 156,
  160, 164, 168, 172, 176, 180, 184, 188,
  192, 196, 200, 204, 208, 212, 216, 220,
  224, 228, 232, 236, 240, 244, 249, 255,
};

int vp9_quantizer_to_qindex(int quantizer) {
  return quantizer_to_qindex[quantizer];
}

int vp9_qindex_to_quantizer(int qindex) {
  int quantizer;

  for (quantizer = 0; quantizer < 64; ++quantizer)
    if (quantizer_to_qindex[quantizer] >= qindex)
      return quantizer;

  return 63;
}
