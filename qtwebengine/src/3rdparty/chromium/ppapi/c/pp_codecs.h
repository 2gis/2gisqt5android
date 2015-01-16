/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_codecs.idl modified Tue Jun 10 13:32:45 2014. */

#ifndef PPAPI_C_PP_CODECS_H_
#define PPAPI_C_PP_CODECS_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 * Video profiles.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_VIDEOPROFILE_H264BASELINE = 0,
  PP_VIDEOPROFILE_H264MAIN = 1,
  PP_VIDEOPROFILE_H264EXTENDED = 2,
  PP_VIDEOPROFILE_H264HIGH = 3,
  PP_VIDEOPROFILE_H264HIGH10PROFILE = 4,
  PP_VIDEOPROFILE_H264HIGH422PROFILE = 5,
  PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE = 6,
  PP_VIDEOPROFILE_H264SCALABLEBASELINE = 7,
  PP_VIDEOPROFILE_H264SCALABLEHIGH = 8,
  PP_VIDEOPROFILE_H264STEREOHIGH = 9,
  PP_VIDEOPROFILE_H264MULTIVIEWHIGH = 10,
  PP_VIDEOPROFILE_VP8MAIN = 11,
  PP_VIDEOPROFILE_VP9MAIN = 12,
  PP_VIDEOPROFILE_MAX = PP_VIDEOPROFILE_VP9MAIN
} PP_VideoProfile;
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * Struct describing a decoded video picture. The decoded picture data is stored
 * in the GL texture corresponding to |texture_id|. The plugin can determine
 * which Decode call generated the picture using |decode_id|.
 */
struct PP_VideoPicture {
  /**
   * |decode_id| parameter of the Decode call which generated this picture.
   * See the PPB_VideoDecoder function Decode() for more details.
   */
  uint32_t decode_id;
  /**
   * Texture ID in the plugin's GL context. The plugin can use this to render
   * the decoded picture.
   */
  uint32_t texture_id;
  /**
   * The GL texture target for the decoded picture. Possible values are:
   *   GL_TEXTURE_2D                 (normalized texture coordinates)
   *   GL_TEXTURE_RECTANGLE_ARB      (dimension dependent texture coordinates)
   *
   * The pixel format of the texture is GL_RGBA.
   */
  uint32_t texture_target;
  /**
   * Dimensions of the texture holding the decoded picture.
   */
  struct PP_Size texture_size;
};
/**
 * @}
 */

#endif  /* PPAPI_C_PP_CODECS_H_ */

