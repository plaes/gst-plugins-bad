/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/d3d11/gstd3d11.h>
#include "gstd3d11videosink.h"
#include "gstd3d11upload.h"
#include "gstd3d11download.h"
#include "gstd3d11convert.h"
#include "gstd3d11videosinkbin.h"
#include "gstd3d11shader.h"
#include "gstd3d11compositor.h"
#include "gstd3d11compositorbin.h"
#ifdef HAVE_DXVA_H
#include "gstd3d11h264dec.h"
#include "gstd3d11h265dec.h"
#include "gstd3d11vp9dec.h"
#include "gstd3d11vp8dec.h"
#include "gstd3d11mpeg2dec.h"
#endif
#ifdef HAVE_DXGI_DESKTOP_DUP
#include "gstd3d11desktopdupsrc.h"
#endif
#ifdef HAVE_D3D11_VIDEO_PROC
#include "gstd3d11deinterlace.h"
#endif

GST_DEBUG_CATEGORY (gst_d3d11_debug);
GST_DEBUG_CATEGORY (gst_d3d11_shader_debug);
GST_DEBUG_CATEGORY (gst_d3d11_converter_debug);
GST_DEBUG_CATEGORY (gst_d3d11_plugin_utils_debug);
GST_DEBUG_CATEGORY (gst_d3d11_format_debug);
GST_DEBUG_CATEGORY (gst_d3d11_device_debug);
GST_DEBUG_CATEGORY (gst_d3d11_overlay_compositor_debug);
GST_DEBUG_CATEGORY (gst_d3d11_window_debug);
GST_DEBUG_CATEGORY (gst_d3d11_video_processor_debug);
GST_DEBUG_CATEGORY (gst_d3d11_compositor_debug);

#ifdef HAVE_DXVA_H
GST_DEBUG_CATEGORY (gst_d3d11_h264_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_h265_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_vp9_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_vp8_dec_debug);
GST_DEBUG_CATEGORY (gst_d3d11_mpeg2_dec_debug);
#endif

#ifdef HAVE_DXGI_DESKTOP_DUP
GST_DEBUG_CATEGORY (gst_d3d11_desktop_dup_debug);
#endif

#ifdef HAVE_D3D11_VIDEO_PROC
GST_DEBUG_CATEGORY (gst_d3d11_deinterlace_debug);
#endif

#define GST_CAT_DEFAULT gst_d3d11_debug

static gboolean
plugin_init (GstPlugin * plugin)
{
  GstRank video_sink_rank = GST_RANK_PRIMARY;
  D3D_FEATURE_LEVEL max_feature_level = D3D_FEATURE_LEVEL_9_3;
  guint i;

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_debug, "d3d11", 0, "direct3d 11 plugin");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_shader_debug,
      "d3d11shader", 0, "d3d11shader");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_converter_debug,
      "d3d11converter", 0, "d3d11converter");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_plugin_utils_debug,
      "d3d11pluginutils", 0, "d3d11 plugin utility functions");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_overlay_compositor_debug,
      "d3d11overlaycompositor", 0, "d3d11overlaycompositor");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_window_debug,
      "d3d11window", 0, "d3d11window");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_video_processor_debug,
      "d3d11videoprocessor", 0, "d3d11videoprocessor");
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_compositor_debug,
      "d3d11compositor", 0, "d3d11compositor element");

  if (!gst_d3d11_shader_init ()) {
    GST_WARNING ("Cannot initialize d3d11 shader");
    return TRUE;
  }
#ifdef HAVE_DXVA_H
  /* DXVA2 API is availble since Windows 8 */
  if (gst_d3d11_is_windows_8_or_greater ()) {
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_h264_dec_debug,
        "d3d11h264dec", 0, "Direct3D11 H.264 Video Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_vp9_dec_debug,
        "d3d11vp9dec", 0, "Direct3D11 VP9 Video Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_h265_dec_debug,
        "d3d11h265dec", 0, "Direct3D11 H.265 Video Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_vp8_dec_debug,
        "d3d11vp8dec", 0, "Direct3D11 VP8 Decoder");
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_mpeg2_dec_debug,
        "d3d11mpeg2dec", 0, "Direct3D11 MPEG2 Decoder");
  }
#endif

#ifdef HAVE_D3D11_VIDEO_PROC
  GST_DEBUG_CATEGORY_INIT (gst_d3d11_deinterlace_debug,
      "d3d11deinterlace", 0, "Direct3D11 Deinterlacer");
#endif

  /* Enumerate devices to register decoders per device and to get the highest
   * feature level */
  /* AMD seems supporting up to 12 cards, and 8 for NVIDIA */
  for (i = 0; i < 12; i++) {
    GstD3D11Device *device = NULL;
    ID3D11Device *device_handle;
    D3D_FEATURE_LEVEL feature_level;

    device = gst_d3d11_device_new (i, D3D11_CREATE_DEVICE_BGRA_SUPPORT);
    if (!device)
      break;

    device_handle = gst_d3d11_device_get_device_handle (device);
    feature_level = ID3D11Device_GetFeatureLevel (device_handle);

    if (feature_level > max_feature_level)
      max_feature_level = feature_level;

#ifdef HAVE_DXVA_H
    /* DXVA2 API is availble since Windows 8 */
    if (gst_d3d11_is_windows_8_or_greater ()) {
      GstD3D11Decoder *decoder = NULL;
      gboolean legacy;
      gboolean hardware;

      g_object_get (device, "hardware", &hardware, NULL);
      if (!hardware)
        goto done;

      decoder = gst_d3d11_decoder_new (device);
      if (!decoder)
        goto done;

      legacy = gst_d3d11_decoder_util_is_legacy_device (device);

      gst_d3d11_h264_dec_register (plugin,
          device, decoder, GST_RANK_SECONDARY, legacy);
      if (!legacy) {
        gst_d3d11_h265_dec_register (plugin, device, decoder,
            GST_RANK_SECONDARY);
        gst_d3d11_vp9_dec_register (plugin, device, decoder,
            GST_RANK_SECONDARY);
        gst_d3d11_vp8_dec_register (plugin, device, decoder,
            GST_RANK_SECONDARY);
        gst_d3d11_mpeg2_dec_register (plugin, device, decoder,
            GST_RANK_SECONDARY);
      }

    done:
      gst_clear_object (&decoder);
    }
#endif

#ifdef HAVE_D3D11_VIDEO_PROC
    /* D3D11 video processor API is availble since Windows 8 */
    if (gst_d3d11_is_windows_8_or_greater ()) {
      gboolean hardware;

      g_object_get (device, "hardware", &hardware, NULL);
      if (hardware)
        gst_d3d11_deinterlace_register (plugin, device, GST_RANK_MARGINAL);
    }
#endif

    gst_object_unref (device);
  }

  /* FIXME: Our shader code is not compatible with D3D_FEATURE_LEVEL_9_3
   * or lower. So HLSL compiler cannot understand our shader code and
   * therefore d3d11colorconverter cannot be configured.
   *
   * Known D3D_FEATURE_LEVEL_9_3 driver is
   * "VirtualBox Graphics Adapter (WDDM)"
   * ... and there might be some more old physical devices which don't support
   * D3D_FEATURE_LEVEL_10_0.
   */
  if (max_feature_level < D3D_FEATURE_LEVEL_10_0)
    video_sink_rank = GST_RANK_NONE;

  gst_d3d11_plugin_utils_init (max_feature_level);

  gst_element_register (plugin,
      "d3d11upload", GST_RANK_NONE, GST_TYPE_D3D11_UPLOAD);
  gst_element_register (plugin,
      "d3d11download", GST_RANK_NONE, GST_TYPE_D3D11_DOWNLOAD);
  gst_element_register (plugin,
      "d3d11convert", GST_RANK_NONE, GST_TYPE_D3D11_CONVERT);
  gst_element_register (plugin,
      "d3d11colorconvert", GST_RANK_NONE, GST_TYPE_D3D11_COLOR_CONVERT);
  gst_element_register (plugin,
      "d3d11scale", GST_RANK_NONE, GST_TYPE_D3D11_SCALE);
  gst_element_register (plugin,
      "d3d11videosinkelement", GST_RANK_NONE, GST_TYPE_D3D11_VIDEO_SINK);

  gst_element_register (plugin,
      "d3d11videosink", video_sink_rank, GST_TYPE_D3D11_VIDEO_SINK_BIN);

  gst_element_register (plugin,
      "d3d11compositorelement", GST_RANK_NONE, GST_TYPE_D3D11_COMPOSITOR);
  gst_element_register (plugin,
      "d3d11compositor", GST_RANK_SECONDARY, GST_TYPE_D3D11_COMPOSITOR_BIN);

#ifdef HAVE_DXGI_DESKTOP_DUP
  if (gst_d3d11_is_windows_8_or_greater ()) {
    GST_DEBUG_CATEGORY_INIT (gst_d3d11_desktop_dup_debug,
        "d3d11desktopdupsrc", 0, "d3d11desktopdupsrc");
    gst_element_register (plugin,
        "d3d11desktopdupsrc", GST_RANK_NONE, GST_TYPE_D3D11_DESKTOP_DUP_SRC);
  }
#endif

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    d3d11,
    "Direct3D11 plugin",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
