/* GStreamer
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
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

/**
 * SECTION:element-d3d11upload
 * @title: d3d11upload
 *
 * Upload video frame to Direct3D11 texture memory
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! d3d11upload ! d3d11videosinkelement
 * ```
 *   This pipeline will upload video test frame (system memory) into Direct3D11
 * textures and d3d11videosinkelement will display frames on screen.
 *
 * Since: 1.18
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstd3d11upload.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_upload_debug);
#define GST_CAT_DEFAULT gst_d3d11_upload_debug

static GstStaticCaps sink_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_D3D11_ALL_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_ALL_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY,
        GST_D3D11_ALL_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY
        "," GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_ALL_FORMATS));

static GstStaticCaps src_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_ALL_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_ALL_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE (GST_D3D11_ALL_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_ALL_FORMATS));

struct _GstD3D11Upload
{
  GstD3D11BaseFilter parent;

  GstBuffer *staging_buffer;
};

#define gst_d3d11_upload_parent_class parent_class
G_DEFINE_TYPE (GstD3D11Upload, gst_d3d11_upload, GST_TYPE_D3D11_BASE_FILTER);

static void gst_d3d11_upload_dispose (GObject * object);
static gboolean gst_d3d11_upload_stop (GstBaseTransform * trans);
static gboolean gst_d3d11_upload_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static GstCaps *gst_d3d11_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_d3d11_upload_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean gst_d3d11_upload_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static GstFlowReturn gst_d3d11_upload_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_d3d11_upload_set_info (GstD3D11BaseFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);

static void
gst_d3d11_upload_class_init (GstD3D11UploadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstD3D11BaseFilterClass *bfilter_class = GST_D3D11_BASE_FILTER_CLASS (klass);
  GstCaps *caps;

  gobject_class->dispose = gst_d3d11_upload_dispose;

  caps = gst_d3d11_get_updated_template_caps (&sink_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  caps = gst_d3d11_get_updated_template_caps (&src_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 uploader", "Filter/Video",
      "Uploads data into Direct3D11 texture memory",
      "Seungha Yang <seungha.yang@navercorp.com>");

  trans_class->passthrough_on_same_caps = TRUE;

  trans_class->stop = GST_DEBUG_FUNCPTR (gst_d3d11_upload_stop);
  trans_class->sink_event = GST_DEBUG_FUNCPTR (gst_d3d11_upload_sink_event);
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_upload_transform_caps);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_upload_propose_allocation);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_upload_decide_allocation);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_d3d11_upload_transform);

  bfilter_class->set_info = GST_DEBUG_FUNCPTR (gst_d3d11_upload_set_info);

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_upload_debug,
      "d3d11upload", 0, "d3d11upload Element");
}

static void
gst_d3d11_upload_init (GstD3D11Upload * upload)
{
}

static void
gst_d3d11_upload_dispose (GObject * object)
{
  GstD3D11Upload *self = GST_D3D11_UPLOAD (object);

  gst_clear_buffer (&self->staging_buffer);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static gboolean
gst_d3d11_upload_stop (GstBaseTransform * trans)
{
  GstD3D11Upload *self = GST_D3D11_UPLOAD (trans);

  gst_clear_buffer (&self->staging_buffer);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (trans);
}

static gboolean
gst_d3d11_upload_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  GstD3D11Upload *self = GST_D3D11_UPLOAD (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      /* We don't need to hold this staging buffer after eos */
      gst_clear_buffer (&self->staging_buffer);
      break;
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->sink_event (trans, event);
}

static GstCaps *
_set_caps_features (const GstCaps * caps, const gchar * feature_name)
{
  guint i, j, m, n;
  GstCaps *tmp;
  GstCapsFeatures *overlay_feature =
      gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

  tmp = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    GstCapsFeatures *features, *orig_features;
    GstStructure *s = gst_caps_get_structure (caps, i);

    orig_features = gst_caps_get_features (caps, i);
    features = gst_caps_features_new (feature_name, NULL);

    if (gst_caps_features_is_any (orig_features)) {
      gst_caps_append_structure_full (tmp, gst_structure_copy (s),
          gst_caps_features_copy (features));

      if (!gst_caps_features_contains (features,
              GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION))
        gst_caps_features_add (features,
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
    } else {
      m = gst_caps_features_get_size (orig_features);
      for (j = 0; j < m; j++) {
        const gchar *feature = gst_caps_features_get_nth (orig_features, j);

        /* if we already have the features */
        if (gst_caps_features_contains (features, feature))
          continue;

        if (g_strcmp0 (feature, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY) == 0)
          continue;

        if (gst_caps_features_contains (overlay_feature, feature)) {
          gst_caps_features_add (features, feature);
        }
      }
    }

    gst_caps_append_structure_full (tmp, gst_structure_copy (s), features);
  }

  gst_caps_features_free (overlay_feature);

  return tmp;
}

static GstCaps *
gst_d3d11_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result, *tmp;

  GST_DEBUG_OBJECT (trans,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  if (direction == GST_PAD_SINK) {
    tmp = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY);
    tmp = gst_caps_merge (gst_caps_ref (caps), tmp);
  } else {
    GstCaps *newcaps;
    tmp = gst_caps_ref (caps);
    newcaps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
    tmp = gst_caps_merge (tmp, newcaps);
  }

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_d3d11_upload_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstVideoInfo info;
  GstBufferPool *pool;
  GstCaps *caps;
  guint size;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  /* passthrough, we're done */
  if (decide_query == NULL)
    return TRUE;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstCapsFeatures *features;
    GstStructure *config;
    gboolean is_d3d11 = FALSE;

    features = gst_caps_get_features (caps, 0);

    if (features && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
      GST_DEBUG_OBJECT (filter, "upstream support d3d11 memory");
      pool = gst_d3d11_buffer_pool_new (filter->device);
      is_d3d11 = TRUE;
    } else {
      pool = gst_video_buffer_pool_new ();
    }

    config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);

    /* d3d11 pool does not support video alignment */
    if (!is_d3d11) {
      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    }

    size = GST_VIDEO_INFO_SIZE (&info);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;

    /* d3d11 buffer pool might update buffer size by self */
    if (is_d3d11)
      size = GST_D3D11_BUFFER_POOL (pool)->buffer_size;

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
    gst_object_unref (pool);
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
config_failed:
  {
    GST_ERROR_OBJECT (filter, "failed to set config");
    gst_object_unref (pool);
    return FALSE;
  }
}

static gboolean
gst_d3d11_upload_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstCaps *outcaps = NULL;
  GstBufferPool *pool = NULL;
  guint size, min, max;
  GstStructure *config;
  gboolean update_pool = FALSE;
  GstVideoInfo vinfo;

  gst_query_parse_allocation (query, &outcaps, NULL);

  if (!outcaps)
    return FALSE;

  gst_video_info_from_caps (&vinfo, outcaps);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool && !GST_IS_D3D11_BUFFER_POOL (pool)) {
      gst_object_unref (pool);
      pool = NULL;
    }

    update_pool = TRUE;
  } else {
    size = GST_VIDEO_INFO_SIZE (&vinfo);
    min = max = 0;
  }

  if (!pool) {
    GST_DEBUG_OBJECT (trans, "create our pool");

    pool = gst_d3d11_buffer_pool_new (filter->device);
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);

  gst_buffer_pool_set_config (pool, config);

  /* update size with calculated one */
  size = GST_D3D11_BUFFER_POOL (pool)->buffer_size;

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);
}

static gboolean
gst_d3d11_upload_can_use_staging_buffer (GstD3D11Upload * self,
    GstBuffer * outbuf)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (self);
  ID3D11Device *device_handle =
      gst_d3d11_device_get_device_handle (filter->device);

  if (!gst_d3d11_buffer_can_access_device (outbuf, device_handle))
    return FALSE;

  if (self->staging_buffer)
    return TRUE;

  self->staging_buffer = gst_d3d11_allocate_staging_buffer_for (outbuf,
      &filter->out_info, TRUE);

  if (!self->staging_buffer) {
    GST_WARNING_OBJECT (self, "Couldn't allocate staging buffer");
    return FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_d3d11_upload_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstD3D11Upload *self = GST_D3D11_UPLOAD (trans);
  GstVideoFrame in_frame, out_frame;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean use_staging_buf;
  GstBuffer *target_outbuf = outbuf;
  guint i;

  use_staging_buf = gst_d3d11_upload_can_use_staging_buffer (self, outbuf);

  if (use_staging_buf) {
    GST_TRACE_OBJECT (self, "Copy input buffer to staging buffer");
    target_outbuf = self->staging_buffer;
  }

  if (!gst_video_frame_map (&in_frame, &filter->in_info, inbuf,
          (GstMapFlags) (GST_MAP_READ | GST_VIDEO_FRAME_MAP_FLAG_NO_REF)))
    goto invalid_buffer;

  if (!gst_video_frame_map (&out_frame, &filter->out_info, target_outbuf,
          (GstMapFlags) (GST_MAP_WRITE | GST_VIDEO_FRAME_MAP_FLAG_NO_REF))) {
    gst_video_frame_unmap (&in_frame);
    goto invalid_buffer;
  }

  for (i = 0; i < GST_VIDEO_FRAME_N_PLANES (&in_frame); i++) {
    if (!gst_video_frame_copy_plane (&out_frame, &in_frame, i)) {
      GST_ERROR_OBJECT (filter, "Couldn't copy plane %d", i);
      ret = GST_FLOW_ERROR;
      break;
    }
  }

  gst_video_frame_unmap (&out_frame);
  gst_video_frame_unmap (&in_frame);

  /* Copy staging texture to d3d11 texture */
  if (use_staging_buf) {
    if (!gst_d3d11_buffer_copy_into (outbuf,
            self->staging_buffer, &filter->out_info)) {
      GST_ERROR_OBJECT (self, "Cannot copy staging texture into texture");
      return GST_FLOW_ERROR;
    }
  }

  return ret;

  /* ERRORS */
invalid_buffer:
  {
    GST_ELEMENT_WARNING (filter, CORE, NOT_IMPLEMENTED, (NULL),
        ("invalid video buffer received"));
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_d3d11_upload_set_info (GstD3D11BaseFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info)
{
  GstD3D11Upload *self = GST_D3D11_UPLOAD (filter);

  gst_clear_buffer (&self->staging_buffer);

  return TRUE;
}
