/* GStreamer
 *
 * Copyright (C) 2018 Sebastian Dröge <sebastian@centricular.com>
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
#include <gst/video/video.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

#include <string.h>

enum CheckConversionFlags
{
  FLAG_NONE,
  FLAG_SEND_EOS = 1,
};

GST_START_TEST (cdp_requires_framerate)
{
  GstHarness *h;
  GstBuffer *buffer;
  GstMapInfo map;

  h = gst_harness_new ("ccconverter");

  /* Enforce conversion to CDP */
  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cdp");

  /* Try without a framerate first, this has to fail */
  gst_harness_set_src_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data");

  buffer = gst_buffer_new_and_alloc (3);
  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  map.data[0] = 0xfc;
  map.data[1] = 0x80;
  map.data[2] = 0x80;
  gst_buffer_unmap (buffer, &map);
  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_NOT_NEGOTIATED);

  /* Now set a framerate only on the sink caps, this should still fail:
   * We can't come up with a framerate
   */
  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)30/1");

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_NOT_NEGOTIATED);

  /* Then try with a change of framerate, this should work */
  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cdp");
  gst_harness_set_src_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data,framerate=(fraction)30/1");

  fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (framerate_passthrough)
{
  GstHarness *h;
  GstBuffer *buffer;
  GstMapInfo map;
  GstCaps *caps, *expected_caps;

  h = gst_harness_new ("ccconverter");

  gst_harness_set_src_caps_str (h,
      "closedcaption/x-cea-608,format=(string)s334-1a,framerate=(fraction)30/1");

  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data");

  buffer = gst_buffer_new_and_alloc (3);
  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  map.data[0] = 0x00;
  map.data[1] = 0x80;
  map.data[2] = 0x80;
  gst_buffer_unmap (buffer, &map);

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_OK);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps);
  expected_caps =
      gst_caps_from_string
      ("closedcaption/x-cea-708,format=(string)cc_data,framerate=(fraction)30/1");
  gst_check_caps_equal (caps, expected_caps);
  gst_caps_unref (caps);
  gst_caps_unref (expected_caps);

  /* Now try between the same formats, should still pass through */
  gst_harness_set_src_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data,framerate=(fraction)30/1");

  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data");

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_OK);

  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps);
  expected_caps =
      gst_caps_from_string
      ("closedcaption/x-cea-708,format=(string)cc_data,framerate=(fraction)30/1");
  gst_check_caps_equal (caps, expected_caps);
  gst_caps_unref (caps);
  gst_caps_unref (expected_caps);

  /* And another time with the same format but only framerate on the output
   * side. This should fail as we can't just come up with a framerate! */
  gst_harness_set_src_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data");

  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data,framerate=(fraction)30/1");

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_NOT_NEGOTIATED);

  /* Now try cdp -> cc_data with framerate passthrough */
  gst_harness_set_src_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)30/1");

  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data");

  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_OK);

  gst_buffer_unref (buffer);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (framerate_changes)
{
  GstHarness *h;
  GstBuffer *buffer;
  GstMapInfo map;

  h = gst_harness_new ("ccconverter");

  buffer = gst_buffer_new_and_alloc (3);
  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  map.data[0] = 0x00;
  map.data[1] = 0x80;
  map.data[2] = 0x80;
  gst_buffer_unmap (buffer, &map);

  /* success case */
  gst_harness_set_src_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cc_data,framerate=(fraction)30/1");
  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1");
  fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
      GST_FLOW_OK);

  /* test an invalid cdp framerate */
  gst_harness_set_sink_caps_str (h,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)1111/1");
  fail_unless_equals_int (gst_harness_push (h, buffer),
      GST_FLOW_NOT_NEGOTIATED);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (framerate_invalid_format)
{
  GstHarness *h;
  GstBuffer *buffer;
  GstMapInfo map;
  guint i, j;

  const gchar *failure_caps[] = {
    /* all of these combinations should fail with different framerates */
    "closedcaption/x-cea-608,format=(string)raw",
    "closedcaption/x-cea-608,format=(string)s334-1a",
    "closedcaption/x-cea-708,format=(string)cc_data",
  };

  h = gst_harness_new ("ccconverter");

  buffer = gst_buffer_new_and_alloc (3);
  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  map.data[0] = 0x00;
  map.data[1] = 0x80;
  map.data[2] = 0x80;
  gst_buffer_unmap (buffer, &map);

  /* framerate conversion failure cases */
  for (i = 0; i < G_N_ELEMENTS (failure_caps); i++) {
    for (j = 0; j < G_N_ELEMENTS (failure_caps); j++) {
      gchar *srccaps, *sinkcaps;

      srccaps =
          g_strdup_printf ("%s%s", failure_caps[i],
          ",framerate=(fraction)30/1");
      sinkcaps =
          g_strdup_printf ("%s%s", failure_caps[i],
          ",framerate=(fraction)60/1");

      GST_INFO ("attempting conversion from %s", srccaps);
      GST_INFO ("                        to %s", sinkcaps);

      gst_harness_set_src_caps_str (h, srccaps);
      gst_harness_set_sink_caps_str (h, sinkcaps);
      fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buffer)),
          GST_FLOW_NOT_NEGOTIATED);

      g_free (srccaps);
      g_free (sinkcaps);
    }
  }

  gst_buffer_unref (buffer);
  gst_harness_teardown (h);
}

GST_END_TEST;

static void
check_conversion_multiple (guint n_in, const guint8 ** in, guint * in_len,
    guint n_out, const guint8 ** out, guint * out_len, const gchar * in_caps,
    const gchar * out_caps, const GstVideoTimeCode ** in_tc,
    const GstVideoTimeCode ** out_tc, enum CheckConversionFlags flags)
{
  GstHarness *h;
  GstBuffer *buffer;
  GstVideoTimeCodeMeta *out_tc_meta;
  int i = 0;

  h = gst_harness_new ("ccconverter");

  gst_harness_set_src_caps_str (h, in_caps);
  gst_harness_set_sink_caps_str (h, out_caps);

  for (i = 0; i < n_in; i++) {
    buffer =
        gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, (gpointer) in[i],
        in_len[i], 0, in_len[i], NULL, NULL);
    GST_INFO ("pushing buffer %u %" GST_PTR_FORMAT, i, buffer);
    if (in_tc && in_tc[i])
      gst_buffer_add_video_time_code_meta (buffer, in_tc[i]);
    fail_unless_equals_int (gst_harness_push (h, buffer), GST_FLOW_OK);
  }

  if (flags & FLAG_SEND_EOS)
    fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  for (i = 0; i < n_out; i++) {
    buffer = gst_harness_pull (h);

    GST_INFO ("pulled buffer %u %" GST_PTR_FORMAT, i, buffer);
    fail_unless (buffer != NULL);
    gst_check_buffer_data (buffer, out[i], out_len[i]);
    out_tc_meta = gst_buffer_get_video_time_code_meta (buffer);
    fail_if (out_tc_meta == NULL && out_tc != NULL && out_tc[i] != NULL);
    if (out_tc_meta && out_tc && out_tc[i])
      fail_unless (gst_video_time_code_compare (&out_tc_meta->tc,
              out_tc[i]) == 0);

    gst_buffer_unref (buffer);
  }

  gst_harness_teardown (h);
}

static void
check_conversion (const guint8 * in, guint in_len, const guint8 * out,
    guint out_len, const gchar * in_caps, const gchar * out_caps,
    const GstVideoTimeCode * in_tc, const GstVideoTimeCode * out_tc)
{
  check_conversion_multiple (1, &in, &in_len, 1, &out, &out_len, in_caps,
      out_caps, &in_tc, &out_tc, 0);
}

static void
check_conversion_tc_passthrough (const guint8 * in, guint in_len,
    const guint8 * out, guint out_len, const gchar * in_caps,
    const gchar * out_caps)
{
  GstVideoTimeCode tc;
  gst_video_time_code_init (&tc, 30, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE,
      1, 2, 3, 4, 0);
  check_conversion (in, in_len, out, out_len, in_caps, out_caps, &tc, &tc);
  gst_video_time_code_clear (&tc);
}

GST_START_TEST (convert_cea608_raw_cea608_s334_1a)
{
  const guint8 in[] = { 0x80, 0x80 };
  const guint8 out[] = { 0x80, 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-608,format=(string)raw",
      "closedcaption/x-cea-608,format=(string)s334-1a");
}

GST_END_TEST;

GST_START_TEST (convert_cea608_raw_cea708_cc_data)
{
  const guint8 in[] = { 0x80, 0x80 };
  const guint8 out[] = { 0xfc, 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-608,format=(string)raw",
      "closedcaption/x-cea-708,format=(string)cc_data");
}

GST_END_TEST;

GST_START_TEST (convert_cea608_raw_cea708_cdp)
{
  const guint8 in[] = { 0x80, 0x80 };
  const guint8 out[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea, 0xfc, 0x80, 0x80,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x00, 0x6e
  };
  check_conversion (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-608,format=(string)raw,framerate=(fraction)60/1",
      "closedcaption/x-cea-708,format=(string)cdp", NULL, NULL);
}

GST_END_TEST;

GST_START_TEST (convert_cea608_s334_1a_cea608_raw)
{
  const guint8 in[] = { 0x80, 0x80, 0x80, 0x00, 0x80, 0x80 };
  const guint8 out[] = { 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-608,format=(string)s334-1a",
      "closedcaption/x-cea-608,format=(string)raw");
}

GST_END_TEST;

GST_START_TEST (convert_cea608_s334_1a_cea608_raw_too_big)
{
  const guint8 in[] =
      { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x80,
    0x80
  };
  const guint8 out[] = { 0x80, 0x80, 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-608,format=(string)s334-1a",
      "closedcaption/x-cea-608,format=(string)raw");
}

GST_END_TEST;

GST_START_TEST (convert_cea608_s334_1a_cea708_cc_data)
{
  const guint8 in[] = { 0x80, 0x80, 0x80, 0x00, 0x80, 0x80 };
  const guint8 out[] = { 0xfc, 0x80, 0x80, 0xfd, 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-608,format=(string)s334-1a",
      "closedcaption/x-cea-708,format=(string)cc_data");
}

GST_END_TEST;

GST_START_TEST (convert_cea608_s334_1a_cea708_cdp)
{
  const guint8 in[] = { 0x80, 0x80, 0x80, 0x00, 0x80, 0x80 };
  const guint8 out[] =
      { 0x96, 0x69, 0x49, 0x5f, 0x43, 0x00, 0x00, 0x72, 0xf4, 0xfc, 0x80, 0x80,
    0xfd, 0x80, 0x80, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x74, 0x00, 0x00, 0xaf
  };
  check_conversion (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-608,format=(string)s334-1a,framerate=(fraction)30/1",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)30/1",
      NULL, NULL);
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cc_data_cea608_raw)
{
  const guint8 in[] = { 0xfc, 0x80, 0x80, 0xfe, 0x80, 0x80 };
  const guint8 out[] = { 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-708,format=(string)cc_data",
      "closedcaption/x-cea-608,format=(string)raw");
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cc_data_cea608_s334_1a)
{
  const guint8 in[] = { 0xfc, 0x80, 0x80, 0xfe, 0x80, 0x80 };
  const guint8 out[] = { 0x80, 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-708,format=(string)cc_data",
      "closedcaption/x-cea-608,format=(string)s334-1a");
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cc_data_cea708_cdp)
{
  const guint8 in[] = { 0xfc, 0x80, 0x80, 0xfe, 0x80, 0x80 };
  const guint8 out[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea, 0xfc, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x00, 0x6a
  };
  check_conversion (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-708,format=(string)cc_data,framerate=(fraction)60/1",
      "closedcaption/x-cea-708,format=(string)cdp", NULL, NULL);
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea608_raw)
{
  const guint8 in[] =
      { 0x96, 0x69, 0x13, 0x5f, 0x43, 0x00, 0x00, 0x72, 0xe2, 0xfc, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0x74, 0x00, 0x00, 0x8a
  };
  const guint8 out[] = { 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-708,format=(string)cdp",
      "closedcaption/x-cea-608,format=(string)raw");
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea608_s334_1a)
{
  const guint8 in[] =
      { 0x96, 0x69, 0x13, 0x5f, 0x43, 0x00, 0x00, 0x72, 0xe2, 0xfc, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0x74, 0x00, 0x00, 0x8a
  };
  const guint8 out[] = { 0x80, 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-708,format=(string)cdp",
      "closedcaption/x-cea-608,format=(string)s334-1a");
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea708_cc_data)
{
  const guint8 in[] =
      { 0x96, 0x69, 0x13, 0x5f, 0x43, 0x00, 0x00, 0x72, 0xe2, 0xfc, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0x74, 0x00, 0x00, 0x8a
  };
  const guint8 out[] = { 0xfc, 0x80, 0x80, 0xfe, 0x80, 0x80 };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-708,format=(string)cdp",
      "closedcaption/x-cea-708,format=(string)cc_data");
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea708_cc_data_too_big)
{
  /* tests that too large input is truncated */
  const guint8 in[] =
      { 0x96, 0x69, 0x2e, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xeb, 0xfc, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80, 0x74, 0x00, 0x00, 0x8a,
  };
  const guint8 out[] = { 0xfc, 0x80, 0x80, 0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80,
    0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80, 0xfe, 0x80, 0x80
  };
  check_conversion_tc_passthrough (in, sizeof (in), out, sizeof (out),
      "closedcaption/x-cea-708,format=(string)cdp",
      "closedcaption/x-cea-708,format=(string)cc_data");
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea708_cdp_double_framerate)
{
  /* tests that packets are split exactly in half when doubling the framerate */
  const guint8 in1[] =
      { 0x96, 0x69, 0x49, 0x5f, 0x43, 0x00, 0x00, 0x72, 0xf4, 0xfc, 0x01, 0x02,
    0xfc, 0x03, 0x04, 0xfe, 0x05, 0x06, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a,
    0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12,
    0xfe, 0x13, 0x14, 0xfe, 0x15, 0x16, 0xfe, 0x17, 0x18, 0xfe, 0x19, 0x1a,
    0xfe, 0x1b, 0x1c, 0xfe, 0x1d, 0x1e, 0xfe, 0x1f, 0x20, 0xfe, 0x21, 0x22,
    0xfe, 0x23, 0x24, 0xfe, 0x25, 0x26, 0xfe, 0x27, 0x28, 0x74, 0x00, 0x00, 0xd2
  };
  const guint8 *in[] = { in1 };
  guint in_len[] = { sizeof (in1) };
  GstVideoTimeCode in_tc1;
  const GstVideoTimeCode *in_tc[] = { &in_tc1 };

  const guint8 out1[] = { 0x96, 0x69, 0x30, 0x8f, 0xc3, 0x00, 0x00, 0x71, 0xd0,
    0xa0, 0x30, 0x00, 0x72, 0xea, 0xfc, 0x01, 0x02, 0xfe, 0x05, 0x06, 0xfe,
    0x07, 0x08, 0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e, 0xfe,
    0x0f, 0x10, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14, 0xfe, 0x15, 0x16, 0x74,
    0x00, 0x00, 0xbe
  };
  const guint8 out2[] = { 0x96, 0x69, 0x30, 0x8f, 0xc3, 0x00, 0x01, 0x71, 0xd0,
    0xa0, 0x30, 0x10, 0x72, 0xea, 0xfc, 0x03, 0x04, 0xfe, 0x17, 0x18, 0xfe,
    0x19, 0x1a, 0xfe, 0x1b, 0x1c, 0xfe, 0x1d, 0x1e, 0xfe, 0x1f, 0x20, 0xfe,
    0x21, 0x22, 0xfe, 0x23, 0x24, 0xfe, 0x25, 0x26, 0xfe, 0x27, 0x28, 0x74,
    0x00, 0x01, 0x64
  };
  const guint8 *out[] = { out1, out2 };
  guint out_len[] = { sizeof (out1), sizeof (out2) };
  GstVideoTimeCode out_tc1, out_tc2;
  const GstVideoTimeCode *out_tc[] = { &out_tc1, &out_tc2 };

  gst_video_time_code_init (&in_tc1, 30, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 1, 2, 3, 4, 0);
  fail_unless (gst_video_time_code_is_valid (&in_tc1));

  gst_video_time_code_init (&out_tc1, 60, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 1, 2, 3, 8, 0);
  fail_unless (gst_video_time_code_is_valid (&out_tc1));
  gst_video_time_code_init (&out_tc2, 60, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 1, 2, 3, 9, 0);
  fail_unless (gst_video_time_code_is_valid (&out_tc2));

  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)30/1",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      in_tc, out_tc, 0);

  gst_video_time_code_clear (&in_tc1);
  gst_video_time_code_clear (&out_tc1);
  gst_video_time_code_clear (&out_tc2);
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea708_cdp_half_framerate)
{
  /* tests that two input packets are merged together when halving the
   * framerate.  With cc_data compaction! */
  const guint8 in1[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea,
    0xfc, 0x01, 0x02, 0xfe, 0x03, 0x04, 0xfe, 0x05, 0x06, 0xfe, 0x07, 0x08,
    0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10,
    0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14, 0x74, 0x00, 0x00, 0x7a
  };
  const guint8 in2[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x01, 0x72, 0xea,
    0xfc, 0x14, 0x15, 0xfe, 0x16, 0x17, 0xfe, 0x18, 0x19, 0xfe, 0x1a, 0x1b,
    0xfe, 0x1c, 0x1d, 0xfe, 0x1e, 0x1f, 0xfe, 0x20, 0x21, 0xfe, 0x22, 0x23,
    0xfe, 0x24, 0x25, 0xfe, 0x26, 0x27, 0x74, 0x00, 0x01, 0x70
  };
  const guint8 *in[] = { in1, in2 };
  guint in_len[] = { sizeof (in1), sizeof (in2) };
  GstVideoTimeCode in_tc1, in_tc2;
  const GstVideoTimeCode *in_tc[] = { &in_tc1, &in_tc2 };

  const guint8 out1[] =
      { 0x96, 0x69, 0x4e, 0x5f, 0xc3, 0x00, 0x00, 0x71, 0xd0, 0xa0, 0x30, 0x00,
    0x72, 0xf4, 0xfc, 0x01, 0x02, 0xfc, 0x14, 0x15, 0xfe, 0x03, 0x04, 0xfe,
    0x05, 0x06, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c, 0xfe,
    0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14, 0xfe,
    0x16, 0x17, 0xfe, 0x18, 0x19, 0xfe, 0x1a, 0x1b, 0xfe, 0x1c, 0x1d, 0xfe,
    0x1e, 0x1f, 0xfe, 0x20, 0x21, 0xfe, 0x22, 0x23, 0xfe, 0x24, 0x25, 0xfe,
    0x26, 0x27, 0x74, 0x00, 0x00, 0xb2
  };
  const guint8 *out[] = { out1 };
  guint out_len[] = { sizeof (out1) };
  GstVideoTimeCode out_tc1;
  const GstVideoTimeCode *out_tc[] = { &out_tc1 };

  gst_video_time_code_init (&in_tc1, 60, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 1, 2, 3, 8, 0);
  fail_unless (gst_video_time_code_is_valid (&in_tc1));
  gst_video_time_code_init (&in_tc2, 60, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 1, 2, 3, 8, 0);
  fail_unless (gst_video_time_code_is_valid (&in_tc2));

  gst_video_time_code_init (&out_tc1, 30, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 1, 2, 3, 4, 0);
  fail_unless (gst_video_time_code_is_valid (&out_tc1));

  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)30/1",
      in_tc, out_tc, 0);

  gst_video_time_code_clear (&in_tc1);
  gst_video_time_code_clear (&in_tc2);
  gst_video_time_code_clear (&out_tc1);
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea708_cdp_max_merge)
{
  /* check that 3 high framerate packets can be merged into 1 low framerate
   * packets with the extra data on the third input packet being placed at the
   * beginning of the second output packet */
  const guint8 in1[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea,
    0xfc, 0x01, 0x02, 0xfe, 0x03, 0x04, 0xfe, 0x05, 0x06, 0xfe, 0x07, 0x08,
    0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10,
    0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14, 0x74, 0x00, 0x00, 0x7a
  };
  /* enough input to fully cover two output packets. Extra is discarded */
  const guint8 *in[] = { in1, in1, in1, in1, in1, in1, in1 };
  guint in_len[] =
      { sizeof (in1), sizeof (in1), sizeof (in1), sizeof (in1), sizeof (in1),
    sizeof (in1)
  };

  const guint8 out1[] =
      { 0x96, 0x69, 0x58, 0x1f, 0x43, 0x00, 0x00, 0x72, 0xf9, 0xfc, 0x01, 0x02,
    0xfc, 0x01, 0x02, 0xfc, 0x01, 0x02, 0xfe, 0x03, 0x04, 0xfe, 0x05, 0x06,
    0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e,
    0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14, 0xfe, 0x03, 0x04,
    0xfe, 0x05, 0x06, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c,
    0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14,
    0xfe, 0x03, 0x04, 0xfe, 0x05, 0x06, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a,
    0x74, 0x00, 0x00, 0xc5
  };
  const guint8 out2[] =
      { 0x96, 0x69, 0x58, 0x1f, 0x43, 0x00, 0x01, 0x72, 0xf9, 0xfc, 0x01, 0x02,
    0xfc, 0x01, 0x02, 0xfc, 0x01, 0x02, 0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e,
    0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14, 0xfe, 0x03, 0x04,
    0xfe, 0x05, 0x06, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c,
    0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14,
    0xfe, 0x03, 0x04, 0xfe, 0x05, 0x06, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a,
    0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12,
    0x74, 0x00, 0x01, 0x83
  };
  const guint8 *out[] = { out1, out2 };
  guint out_len[] = { sizeof (out1), sizeof (out2) };
  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)24000/1001",
      NULL, NULL, 0);
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea708_cdp_max_split)
{
  /* test that a low framerate stream produces multiple output packets for a
   * high framerate */
  const guint8 in1[] =
      { 0x96, 0x69, 0x58, 0x1f, 0x43, 0x00, 0x00, 0x72, 0xf9, 0xfc, 0x01, 0x02,
    0xfc, 0x03, 0x04, 0xfc, 0x05, 0x06, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a,
    0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12,
    0xfe, 0x13, 0x14, 0xfe, 0x15, 0x16, 0xfe, 0x17, 0x18, 0xfe, 0x19, 0x1a,
    0xfe, 0x1b, 0x1c, 0xfe, 0x1d, 0x1e, 0xfe, 0x1f, 0x20, 0xfe, 0x21, 0x22,
    0xfe, 0x23, 0x24, 0xfe, 0x25, 0x26, 0xfe, 0x27, 0x28, 0xfe, 0x29, 0x2a,
    0xfe, 0x2b, 0x2c, 0xfe, 0x2d, 0x2e, 0xfe, 0x2f, 0x30, 0xfe, 0x31, 0x32,
    0x74, 0x00, 0x00, 0x12
  };
  const guint8 *in[] = { in1, in1 };
  guint in_len[] = { sizeof (in1), sizeof (in1) };

  const guint8 out1[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea,
    0xfc, 0x01, 0x02, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c,
    0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14,
    0xfe, 0x15, 0x16, 0xfe, 0x17, 0x18, 0x74, 0x00, 0x00, 0x30
  };
  const guint8 out2[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x01, 0x72, 0xea,
    0xfc, 0x03, 0x04, 0xfe, 0x19, 0x1a, 0xfe, 0x1b, 0x1c, 0xfe, 0x1d, 0x1e,
    0xfe, 0x1f, 0x20, 0xfe, 0x21, 0x22, 0xfe, 0x23, 0x24, 0xfe, 0x25, 0x26,
    0xfe, 0x27, 0x28, 0xfe, 0x29, 0x2a, 0x74, 0x00, 0x01, 0xe6
  };
  const guint8 out3[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x02, 0x72, 0xea,
    0xfc, 0x05, 0x06, 0xfe, 0x2b, 0x2c, 0xfe, 0x2d, 0x2e, 0xfe, 0x2f, 0x30,
    0xfe, 0x31, 0x32, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c,
    0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0x74, 0x00, 0x02, 0x54
  };
  const guint8 out4[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x03, 0x72, 0xea,
    0xfc, 0x01, 0x02, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14, 0xfe, 0x15, 0x16,
    0xfe, 0x17, 0x18, 0xfe, 0x19, 0x1a, 0xfe, 0x1b, 0x1c, 0xfe, 0x1d, 0x1e,
    0xfe, 0x1f, 0x20, 0xfe, 0x21, 0x22, 0x74, 0x00, 0x03, 0x76
  };
  const guint8 *out[] = { out1, out2, out3, out4 };
  guint out_len[] =
      { sizeof (out1), sizeof (out2), sizeof (out3), sizeof (out4) };
  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)24000/1001",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      NULL, NULL, 0);
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea708_cdp_max_split_eos)
{
  /* test that a low framerate stream produces multiple output packets for a
   * high framerate and that an EOS will push the pending data */
  const guint8 in1[] =
      { 0x96, 0x69, 0x58, 0x1f, 0x43, 0x00, 0x00, 0x72, 0xf9, 0xfc, 0x01, 0x02,
    0xfc, 0x03, 0x04, 0xfc, 0x05, 0x06, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a,
    0xfe, 0x0b, 0x0c, 0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12,
    0xfe, 0x13, 0x14, 0xfe, 0x15, 0x16, 0xfe, 0x17, 0x18, 0xfe, 0x19, 0x1a,
    0xfe, 0x1b, 0x1c, 0xfe, 0x1d, 0x1e, 0xfe, 0x1f, 0x20, 0xfe, 0x21, 0x22,
    0xfe, 0x23, 0x24, 0xfe, 0x25, 0x26, 0xfe, 0x27, 0x28, 0xfe, 0x29, 0x2a,
    0xfe, 0x2b, 0x2c, 0xfe, 0x2d, 0x2e, 0xfe, 0x2f, 0x30, 0xfe, 0x31, 0x32,
    0x74, 0x00, 0x00, 0x12
  };
  const guint8 *in[] = { in1 };
  guint in_len[] = { sizeof (in1) };

  const guint8 out1[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea,
    0xfc, 0x01, 0x02, 0xfe, 0x07, 0x08, 0xfe, 0x09, 0x0a, 0xfe, 0x0b, 0x0c,
    0xfe, 0x0d, 0x0e, 0xfe, 0x0f, 0x10, 0xfe, 0x11, 0x12, 0xfe, 0x13, 0x14,
    0xfe, 0x15, 0x16, 0xfe, 0x17, 0x18, 0x74, 0x00, 0x00, 0x30
  };
  const guint8 out2[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x01, 0x72, 0xea,
    0xfc, 0x03, 0x04, 0xfe, 0x19, 0x1a, 0xfe, 0x1b, 0x1c, 0xfe, 0x1d, 0x1e,
    0xfe, 0x1f, 0x20, 0xfe, 0x21, 0x22, 0xfe, 0x23, 0x24, 0xfe, 0x25, 0x26,
    0xfe, 0x27, 0x28, 0xfe, 0x29, 0x2a, 0x74, 0x00, 0x01, 0xe6
  };
  const guint8 out3[] = { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x02, 0x72, 0xea,
    0xfc, 0x05, 0x06, 0xfe, 0x2b, 0x2c, 0xfe, 0x2d, 0x2e, 0xfe, 0x2f, 0x30,
    0xfe, 0x31, 0x32, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x74, 0x00, 0x02, 0xdb
  };
  const guint8 *out[] = { out1, out2, out3 };
  guint out_len[] = { sizeof (out1), sizeof (out2), sizeof (out3) };

  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)24000/1001",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      NULL, NULL, FLAG_SEND_EOS);
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cdp_cea708_cdp_from_drop_frame_scaling)
{
  const guint8 in1[] = { 0x96, 0x69, 0x10, 0x7f, 0x43, 0x00, 0x00, 0x72, 0xe1,
    0xfc, 0x80, 0x80, 0x74, 0x00, 0x00, 0x7a
  };
  const guint8 *in[] = { in1, in1 };
  guint in_len[] = { sizeof (in1), sizeof (in1) };
  GstVideoTimeCode in_tc1, in_tc2;
  const GstVideoTimeCode *in_tc[] = { &in_tc1, &in_tc2 };

  const guint8 out1[] =
      { 0x96, 0x69, 0x30, 0x8f, 0xc3, 0x00, 0x00, 0x71, 0xc0, 0x90, 0x12, 0x12,
    0x72, 0xea, 0xfc, 0x80, 0x80, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa,
    0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa,
    0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x74, 0x00, 0x00, 0x04
  };
  const guint8 out2[] =
      { 0x96, 0x69, 0x30, 0x8f, 0xc3, 0x00, 0x01, 0x71, 0xc0, 0xa0, 0x00, 0x00,
    0x72, 0xea, 0xfc, 0x80, 0x80, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa,
    0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa,
    0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x74, 0x00, 0x01, 0x16
  };
  const guint8 *out[] = { out1, out2 };
  guint out_len[] = { sizeof (out1), sizeof (out2) };
  GstVideoTimeCode out_tc1, out_tc2;
  const GstVideoTimeCode *out_tc[] = { &out_tc1, &out_tc2 };

  gst_video_time_code_init (&in_tc1, 60000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 0, 1, 59, 59, 0);
  fail_unless (gst_video_time_code_is_valid (&in_tc1));

  gst_video_time_code_init (&in_tc2, 60000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 0, 2, 0, 4, 0);
  fail_unless (gst_video_time_code_is_valid (&in_tc2));

  gst_video_time_code_init (&out_tc1, 60, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 1, 59, 59, 0);
  fail_unless (gst_video_time_code_is_valid (&out_tc1));

  gst_video_time_code_init (&out_tc2, 60, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 2, 0, 0, 0);
  fail_unless (gst_video_time_code_is_valid (&out_tc2));

  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60000/1001",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      in_tc, out_tc, FLAG_SEND_EOS);

  gst_video_time_code_clear (&in_tc1);
  gst_video_time_code_clear (&out_tc1);
}

GST_END_TEST;

GST_START_TEST (convert_cea708_cc_data_cea708_cdp_double_framerate)
{
  const guint8 in1[] = { 0xfc, 0x80, 0x81, 0xfc, 0x82, 0x83, 0xfe, 0x84, 0x85 };
  const guint8 in2[] = { 0xfc, 0x86, 0x87, 0xfc, 0x88, 0x89, 0xfe, 0x8a, 0x8b };
  const guint8 *in[] = { in1, in2 };
  guint in_len[] = { sizeof (in1), sizeof (in2) };
  const guint8 out1[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea, 0xfc, 0x80, 0x81,
    0xfe, 0x84, 0x85, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x00, 0x60
  };
  const guint8 out2[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x01, 0x72, 0xea, 0xfc, 0x82, 0x83,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x01, 0x67
  };
  const guint8 out3[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x02, 0x72, 0xea, 0xfc, 0x86, 0x87,
    0xfe, 0x8a, 0x8b, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x02, 0x44
  };
  const guint8 out4[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x03, 0x72, 0xea, 0xfc, 0x88, 0x89,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x03, 0x57
  };
  const guint8 *out[] = { out1, out2, out3, out4, };
  guint out_len[] =
      { sizeof (out1), sizeof (out2), sizeof (out3), sizeof (out4), };
  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-708,format=(string)cc_data,framerate=(fraction)30/1",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      NULL, NULL, 0);
}

GST_END_TEST;

GST_START_TEST (convert_cea608_raw_cea708_cdp_double_framerate)
{
  const guint8 in1[] = { 0x80, 0x81, 0x82, 0x83 };
  const guint8 in2[] = { 0x84, 0x85, 0x86, 0x87 };
  const guint8 *in[] = { in1, in2 };
  guint in_len[] = { sizeof (in1), sizeof (in2) };
  const guint8 out1[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea, 0xfc, 0x80, 0x81,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x00, 0x6d
  };
  const guint8 out2[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x01, 0x72, 0xea, 0xfc, 0x82, 0x83,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x01, 0x67
  };
  const guint8 out3[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x02, 0x72, 0xea, 0xfc, 0x84, 0x85,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x02, 0x61
  };
  const guint8 out4[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x03, 0x72, 0xea, 0xfc, 0x86, 0x87,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x03, 0x5b
  };
  const guint8 *out[] = { out1, out2, out3, out4, };
  guint out_len[] =
      { sizeof (out1), sizeof (out2), sizeof (out3), sizeof (out4), };
  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-608,format=(string)raw,framerate=(fraction)30/1",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      NULL, NULL, 0);
}

GST_END_TEST;

GST_START_TEST (convert_cea608_s334_1a_cea708_cdp_double_framerate)
{
  const guint8 in1[] = { 0x80, 0x80, 0x81, 0x00, 0x82, 0x83 };
  const guint8 in2[] = { 0x80, 0x84, 0x85, 0x00, 0x86, 0x87 };
  const guint8 *in[] = { in1, in2 };
  guint in_len[] = { sizeof (in1), sizeof (in2) };
  const guint8 out1[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x00, 0x72, 0xea, 0xfc, 0x80, 0x81,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x00, 0x6d
  };
  const guint8 out2[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x01, 0x72, 0xea, 0xfd, 0x82, 0x83,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x01, 0x66
  };
  const guint8 out3[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x02, 0x72, 0xea, 0xfc, 0x84, 0x85,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x02, 0x61
  };
  const guint8 out4[] =
      { 0x96, 0x69, 0x2b, 0x8f, 0x43, 0x00, 0x03, 0x72, 0xea, 0xfd, 0x86, 0x87,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0x74, 0x00, 0x03, 0x5a
  };
  const guint8 *out[] = { out1, out2, out3, out4, };
  guint out_len[] =
      { sizeof (out1), sizeof (out2), sizeof (out3), sizeof (out4), };
  check_conversion_multiple (G_N_ELEMENTS (in_len), in, in_len,
      G_N_ELEMENTS (out_len), out, out_len,
      "closedcaption/x-cea-608,format=(string)s334-1a,framerate=(fraction)30/1",
      "closedcaption/x-cea-708,format=(string)cdp,framerate=(fraction)60/1",
      NULL, NULL, 0);
}

GST_END_TEST;

static Suite *
ccextractor_suite (void)
{
  Suite *s = suite_create ("ccconverter");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);

  tcase_add_test (tc, cdp_requires_framerate);
  tcase_add_test (tc, framerate_passthrough);
  tcase_add_test (tc, framerate_changes);
  tcase_add_test (tc, framerate_invalid_format);
  tcase_add_test (tc, convert_cea608_raw_cea608_s334_1a);
  tcase_add_test (tc, convert_cea608_raw_cea708_cc_data);
  tcase_add_test (tc, convert_cea608_raw_cea708_cdp);
  tcase_add_test (tc, convert_cea608_s334_1a_cea608_raw);
  tcase_add_test (tc, convert_cea608_s334_1a_cea608_raw_too_big);
  tcase_add_test (tc, convert_cea608_s334_1a_cea708_cc_data);
  tcase_add_test (tc, convert_cea608_s334_1a_cea708_cdp);
  tcase_add_test (tc, convert_cea708_cc_data_cea608_raw);
  tcase_add_test (tc, convert_cea708_cc_data_cea608_s334_1a);
  tcase_add_test (tc, convert_cea708_cc_data_cea708_cdp);
  tcase_add_test (tc, convert_cea708_cdp_cea608_raw);
  tcase_add_test (tc, convert_cea708_cdp_cea608_s334_1a);
  tcase_add_test (tc, convert_cea708_cdp_cea708_cc_data);
  tcase_add_test (tc, convert_cea708_cdp_cea708_cc_data_too_big);
  tcase_add_test (tc, convert_cea708_cdp_cea708_cdp_half_framerate);
  tcase_add_test (tc, convert_cea708_cdp_cea708_cdp_double_framerate);
  tcase_add_test (tc, convert_cea708_cdp_cea708_cdp_max_merge);
  tcase_add_test (tc, convert_cea708_cdp_cea708_cdp_max_split);
  tcase_add_test (tc, convert_cea708_cdp_cea708_cdp_max_split_eos);
  tcase_add_test (tc, convert_cea708_cdp_cea708_cdp_from_drop_frame_scaling);
  tcase_add_test (tc, convert_cea708_cc_data_cea708_cdp_double_framerate);
  tcase_add_test (tc, convert_cea608_raw_cea708_cdp_double_framerate);
  tcase_add_test (tc, convert_cea608_s334_1a_cea708_cdp_double_framerate);

  return s;
}

GST_CHECK_MAIN (ccextractor);
