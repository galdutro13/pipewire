/* GStreamer
 * Copyright (C) 2016 Wim Taymans <wim.taymans@gmail.com>
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

#define _GNU_SOURCE
#include <stdio.h>

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>

#include <spa/utils/type.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include "gstpipewireformat.h"

struct media_type {
  const char *name;
  uint32_t media_type;
  uint32_t media_subtype;
};

static const struct media_type media_type_map[] = {
  { "video/x-raw", SPA_MEDIA_TYPE_video, SPA_MEDIA_SUBTYPE_raw },
  { "audio/x-raw", SPA_MEDIA_TYPE_audio, SPA_MEDIA_SUBTYPE_raw },
  { "image/jpeg", SPA_MEDIA_TYPE_video, SPA_MEDIA_SUBTYPE_mjpg },
  { "video/x-h264", SPA_MEDIA_TYPE_video, SPA_MEDIA_SUBTYPE_h264 },
  { NULL, }
};

static const uint32_t video_format_map[] = {
  SPA_VIDEO_FORMAT_UNKNOWN,
  SPA_VIDEO_FORMAT_ENCODED,
  SPA_VIDEO_FORMAT_I420,
  SPA_VIDEO_FORMAT_YV12,
  SPA_VIDEO_FORMAT_YUY2,
  SPA_VIDEO_FORMAT_UYVY,
  SPA_VIDEO_FORMAT_AYUV,
  SPA_VIDEO_FORMAT_RGBx,
  SPA_VIDEO_FORMAT_BGRx,
  SPA_VIDEO_FORMAT_xRGB,
  SPA_VIDEO_FORMAT_xBGR,
  SPA_VIDEO_FORMAT_RGBA,
  SPA_VIDEO_FORMAT_BGRA,
  SPA_VIDEO_FORMAT_ARGB,
  SPA_VIDEO_FORMAT_ABGR,
  SPA_VIDEO_FORMAT_RGB,
  SPA_VIDEO_FORMAT_BGR,
  SPA_VIDEO_FORMAT_Y41B,
  SPA_VIDEO_FORMAT_Y42B,
  SPA_VIDEO_FORMAT_YVYU,
  SPA_VIDEO_FORMAT_Y444,
  SPA_VIDEO_FORMAT_v210,
  SPA_VIDEO_FORMAT_v216,
  SPA_VIDEO_FORMAT_NV12,
  SPA_VIDEO_FORMAT_NV21,
  SPA_VIDEO_FORMAT_GRAY8,
  SPA_VIDEO_FORMAT_GRAY16_BE,
  SPA_VIDEO_FORMAT_GRAY16_LE,
  SPA_VIDEO_FORMAT_v308,
  SPA_VIDEO_FORMAT_RGB16,
  SPA_VIDEO_FORMAT_BGR16,
  SPA_VIDEO_FORMAT_RGB15,
  SPA_VIDEO_FORMAT_BGR15,
  SPA_VIDEO_FORMAT_UYVP,
  SPA_VIDEO_FORMAT_A420,
  SPA_VIDEO_FORMAT_RGB8P,
  SPA_VIDEO_FORMAT_YUV9,
  SPA_VIDEO_FORMAT_YVU9,
  SPA_VIDEO_FORMAT_IYU1,
  SPA_VIDEO_FORMAT_ARGB64,
  SPA_VIDEO_FORMAT_AYUV64,
  SPA_VIDEO_FORMAT_r210,
  SPA_VIDEO_FORMAT_I420_10BE,
  SPA_VIDEO_FORMAT_I420_10LE,
  SPA_VIDEO_FORMAT_I422_10BE,
  SPA_VIDEO_FORMAT_I422_10LE,
  SPA_VIDEO_FORMAT_Y444_10BE,
  SPA_VIDEO_FORMAT_Y444_10LE,
  SPA_VIDEO_FORMAT_GBR,
  SPA_VIDEO_FORMAT_GBR_10BE,
  SPA_VIDEO_FORMAT_GBR_10LE,
  SPA_VIDEO_FORMAT_NV16,
  SPA_VIDEO_FORMAT_NV24,
  SPA_VIDEO_FORMAT_NV12_64Z32,
  SPA_VIDEO_FORMAT_A420_10BE,
  SPA_VIDEO_FORMAT_A420_10LE,
  SPA_VIDEO_FORMAT_A422_10BE,
  SPA_VIDEO_FORMAT_A422_10LE,
  SPA_VIDEO_FORMAT_A444_10BE,
  SPA_VIDEO_FORMAT_A444_10LE,
  SPA_VIDEO_FORMAT_NV61,
  SPA_VIDEO_FORMAT_P010_10BE,
  SPA_VIDEO_FORMAT_P010_10LE,
  SPA_VIDEO_FORMAT_IYU2,
  SPA_VIDEO_FORMAT_VYUY,
  SPA_VIDEO_FORMAT_GBRA,
  SPA_VIDEO_FORMAT_GBRA_10BE,
  SPA_VIDEO_FORMAT_GBRA_10LE,
  SPA_VIDEO_FORMAT_GBR_12BE,
  SPA_VIDEO_FORMAT_GBR_12LE,
  SPA_VIDEO_FORMAT_GBRA_12BE,
  SPA_VIDEO_FORMAT_GBRA_12LE,
  SPA_VIDEO_FORMAT_I420_12BE,
  SPA_VIDEO_FORMAT_I420_12LE,
  SPA_VIDEO_FORMAT_I422_12BE,
  SPA_VIDEO_FORMAT_I422_12LE,
  SPA_VIDEO_FORMAT_Y444_12BE,
  SPA_VIDEO_FORMAT_Y444_12LE,
};

#if __BYTE_ORDER == __BIG_ENDIAN
#define _FORMAT_LE(fmt)  SPA_AUDIO_FORMAT_ ## fmt ## _OE
#define _FORMAT_BE(fmt)  SPA_AUDIO_FORMAT_ ## fmt
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define _FORMAT_LE(fmt)  SPA_AUDIO_FORMAT_ ## fmt
#define _FORMAT_BE(fmt)  SPA_AUDIO_FORMAT_ ## fmt ## _OE
#endif

static const uint32_t audio_format_map[] = {
  SPA_AUDIO_FORMAT_UNKNOWN,
  SPA_AUDIO_FORMAT_ENCODED,
  SPA_AUDIO_FORMAT_S8,
  SPA_AUDIO_FORMAT_U8,
  _FORMAT_LE (S16),
  _FORMAT_BE (S16),
  _FORMAT_LE (U16),
  _FORMAT_BE (U16),
  _FORMAT_LE (S24_32),
  _FORMAT_BE (S24_32),
  _FORMAT_LE (U24_32),
  _FORMAT_BE (U24_32),
  _FORMAT_LE (S32),
  _FORMAT_BE (S32),
  _FORMAT_LE (U32),
  _FORMAT_BE (U32),
  _FORMAT_LE (S24),
  _FORMAT_BE (S24),
  _FORMAT_LE (U24),
  _FORMAT_BE (U24),
  _FORMAT_LE (S20),
  _FORMAT_BE (S20),
  _FORMAT_LE (U20),
  _FORMAT_BE (U20),
  _FORMAT_LE (S18),
  _FORMAT_BE (S18),
  _FORMAT_LE (U18),
  _FORMAT_BE (U18),
  _FORMAT_LE (F32),
  _FORMAT_BE (F32),
  _FORMAT_LE (F64),
  _FORMAT_BE (F64),
};

typedef struct {
  struct spa_pod_builder b;
  const struct media_type *type;
  uint32_t id;
  const GstCapsFeatures *cf;
  const GstStructure *cs;
  GPtrArray *array;
} ConvertData;

static const struct media_type *
find_media_types (const char *name)
{
  int i;
  for (i = 0; media_type_map[i].name; i++) {
    if (!strcmp (media_type_map[i].name, name))
      return &media_type_map[i];
  }
  return NULL;
}

static int find_index(const uint32_t *items, int n_items, uint32_t id)
{
  int i;
  for (i = 0; i < n_items; i++)
    if (items[i] == id)
      return i;
  return -1;
}

static const char *
get_nth_string (const GValue *val, int idx)
{
  const GValue *v = NULL;
  GType type = G_VALUE_TYPE (val);

  if (type == G_TYPE_STRING && idx == 0)
    v = val;
  else if (type == GST_TYPE_LIST) {
    GArray *array = g_value_peek_pointer (val);
    if (idx < array->len + 1) {
      v = &g_array_index (array, GValue, SPA_MAX (idx - 1, 0));
    }
  }
  if (v)
    return g_value_get_string (v);

  return NULL;
}

static bool
get_nth_int (const GValue *val, int idx, int *res)
{
  const GValue *v = NULL;
  GType type = G_VALUE_TYPE (val);

  if (type == G_TYPE_INT && idx == 0) {
    v = val;
  } else if (type == GST_TYPE_INT_RANGE) {
    if (idx == 0 || idx == 1) {
      *res = gst_value_get_int_range_min (val);
      return true;
    } else if (idx == 2) {
      *res = gst_value_get_int_range_max (val);
      return true;
    }
  } else if (type == GST_TYPE_LIST) {
    GArray *array = g_value_peek_pointer (val);
    if (idx < array->len + 1) {
      v = &g_array_index (array, GValue, SPA_MAX (idx - 1, 0));
    }
  }
  if (v) {
    *res = g_value_get_int (v);
    return true;
  }
  return false;
}

static gboolean
get_nth_fraction (const GValue *val, int idx, struct spa_fraction *f)
{
  const GValue *v = NULL;
  GType type = G_VALUE_TYPE (val);

  if (type == GST_TYPE_FRACTION && idx == 0) {
    v = val;
  } else if (type == GST_TYPE_FRACTION_RANGE) {
    if (idx == 0 || idx == 1) {
      v = gst_value_get_fraction_range_min (val);
    } else if (idx == 2) {
      v = gst_value_get_fraction_range_max (val);
    }
  } else if (type == GST_TYPE_LIST) {
    GArray *array = g_value_peek_pointer (val);
    if (idx < array->len + 1) {
      v = &g_array_index (array, GValue, SPA_MAX (idx-1, 0));
    }
  }
  if (v) {
    f->num = gst_value_get_fraction_numerator (v);
    f->denom = gst_value_get_fraction_denominator (v);
    return true;
  }
  return false;
}

static gboolean
get_nth_rectangle (const GValue *width, const GValue *height, int idx, struct spa_rectangle *r)
{
  const GValue *w = NULL, *h = NULL;
  GType wt = G_VALUE_TYPE (width);
  GType ht = G_VALUE_TYPE (height);

  if (wt == G_TYPE_INT && ht == G_TYPE_INT && idx == 0) {
    w = width;
    h = height;
  } else if (wt == GST_TYPE_INT_RANGE && ht == GST_TYPE_INT_RANGE) {
    if (idx == 0 || idx == 1) {
      r->width = gst_value_get_int_range_min (width);
      r->height = gst_value_get_int_range_min (height);
      return true;
    } else if (idx == 2) {
      r->width = gst_value_get_int_range_max (width);
      r->height = gst_value_get_int_range_max (height);
      return true;
    }
  } else if (wt == GST_TYPE_LIST && ht == GST_TYPE_LIST) {
    GArray *wa = g_value_peek_pointer (width);
    GArray *ha = g_value_peek_pointer (height);
    if (idx < wa->len + 1)
      w = &g_array_index (wa, GValue, SPA_MAX (idx-1, 0));
    if (idx < ha->len + 1)
      h = &g_array_index (ha, GValue, SPA_MAX (idx-1, 0));
  }
  if (w && h) {
    r->width = g_value_get_int (w);
    r->height = g_value_get_int (h);
    return true;
  }
  return false;
}

static const uint32_t
get_range_type (const GValue *val)
{
  GType type = G_VALUE_TYPE (val);

  if (type == GST_TYPE_LIST)
    return SPA_POD_PROP_RANGE_ENUM;
  if (type == GST_TYPE_DOUBLE_RANGE || type == GST_TYPE_FRACTION_RANGE)
    return SPA_POD_PROP_RANGE_MIN_MAX;
  if (type == GST_TYPE_INT_RANGE) {
    if (gst_value_get_int_range_step (val) == 1)
      return SPA_POD_PROP_RANGE_MIN_MAX;
    else
      return SPA_POD_PROP_RANGE_STEP;
  }
  if (type == GST_TYPE_INT64_RANGE) {
    if (gst_value_get_int64_range_step (val) == 1)
      return SPA_POD_PROP_RANGE_MIN_MAX;
    else
      return SPA_POD_PROP_RANGE_STEP;
  }
  return SPA_POD_PROP_RANGE_NONE;
}

static const uint32_t
get_range_type2 (const GValue *v1, const GValue *v2)
{
  uint32_t r1, r2;

  r1 = get_range_type (v1);
  r2 = get_range_type (v2);

  if (r1 == r2)
    return r1;
  if (r1 == SPA_POD_PROP_RANGE_STEP || r2 == SPA_POD_PROP_RANGE_STEP)
    return SPA_POD_PROP_RANGE_STEP;
  if (r1 == SPA_POD_PROP_RANGE_MIN_MAX || r2 == SPA_POD_PROP_RANGE_MIN_MAX)
    return SPA_POD_PROP_RANGE_MIN_MAX;
  return SPA_POD_PROP_RANGE_MIN_MAX;
}

static gboolean
handle_video_fields (ConvertData *d)
{
  const GValue *value, *value2;
  int i;
  struct spa_pod_prop *prop;

  value = gst_structure_get_value (d->cs, "format");
  if (value) {
    const char *v;
    int idx;
    for (i = 0; (v = get_nth_string (value, i)); i++) {
      if (i == 0)
        spa_pod_builder_push_prop (&d->b,
                                   SPA_FORMAT_VIDEO_format,
                                   get_range_type (value));

      idx = gst_video_format_from_string (v);
      if (idx < SPA_N_ELEMENTS (video_format_map))
        spa_pod_builder_id (&d->b, video_format_map[idx]);
    }
    prop = spa_pod_builder_pop(&d->b);
    if (i > 1)
      prop->body.flags |= SPA_POD_PROP_FLAG_UNSET;
  }
  value = gst_structure_get_value (d->cs, "width");
  value2 = gst_structure_get_value (d->cs, "height");
  if (value || value2) {
    struct spa_rectangle v;
    for (i = 0; get_nth_rectangle (value, value2, i, &v); i++) {
      if (i == 0)
        spa_pod_builder_push_prop (&d->b,
                                   SPA_FORMAT_VIDEO_size,
                                   get_range_type2 (value, value2));

      spa_pod_builder_rectangle (&d->b, v.width, v.height);
    }
    prop = spa_pod_builder_pop(&d->b);
    if (i > 1)
      prop->body.flags |= SPA_POD_PROP_FLAG_UNSET;
  }

  value = gst_structure_get_value (d->cs, "framerate");
  if (value) {
    struct spa_fraction v;
    for (i = 0; get_nth_fraction (value, i, &v); i++) {
      if (i == 0)
        spa_pod_builder_push_prop (&d->b,
                                   SPA_FORMAT_VIDEO_framerate,
                                   get_range_type (value));

      spa_pod_builder_fraction (&d->b, v.num, v.denom);
    }
    prop = spa_pod_builder_pop(&d->b);
    if (i > 1)
      prop->body.flags |= SPA_POD_PROP_FLAG_UNSET;
  }

  value = gst_structure_get_value (d->cs, "max-framerate");
  if (value) {
    struct spa_fraction v;
    for (i = 0; get_nth_fraction (value, i, &v); i++) {
      if (i == 0)
        spa_pod_builder_push_prop (&d->b,
                                   SPA_FORMAT_VIDEO_maxFramerate,
                                   get_range_type (value));

      spa_pod_builder_fraction (&d->b, v.num, v.denom);
    }
    prop = spa_pod_builder_pop(&d->b);
    if (i > 1)
      prop->body.flags |= SPA_POD_PROP_FLAG_UNSET;
  }
  return TRUE;
}

static gboolean
handle_audio_fields (ConvertData *d)
{
  const GValue *value;
  struct spa_pod_prop *prop;
  int i = 0;

  value = gst_structure_get_value (d->cs, "format");
  if (value) {
    const char *v;
    int idx;
    for (i = 0; (v = get_nth_string (value, i)); i++) {
      if (i == 0)
        spa_pod_builder_push_prop (&d->b,
                                   SPA_FORMAT_AUDIO_format,
                                   get_range_type (value));

      idx = gst_audio_format_from_string (v);
      if (idx < SPA_N_ELEMENTS (audio_format_map))
        spa_pod_builder_id (&d->b, audio_format_map[idx]);
    }
    prop = spa_pod_builder_pop(&d->b);
    if (i > 1)
      prop->body.flags |= SPA_POD_PROP_FLAG_UNSET;
  }

  value = gst_structure_get_value (d->cs, "layout");
  if (value) {
    const char *v;
    for (i = 0; (v = get_nth_string (value, i)); i++) {
      enum spa_audio_layout layout;

      if (!strcmp (v, "interleaved"))
        layout = SPA_AUDIO_LAYOUT_INTERLEAVED;
      else if (!strcmp (v, "non-interleaved"))
        layout = SPA_AUDIO_LAYOUT_NON_INTERLEAVED;
      else
        break;

      if (i == 0)
        spa_pod_builder_push_prop (&d->b,
                                   SPA_FORMAT_AUDIO_layout,
                                   get_range_type (value));

      spa_pod_builder_enum (&d->b, layout);
    }
    prop = spa_pod_builder_pop(&d->b);
    if (i > 1)
      prop->body.flags |= SPA_POD_PROP_FLAG_UNSET;
  }
  value = gst_structure_get_value (d->cs, "rate");
  if (value) {
    int v;
    for (i = 0; get_nth_int (value, i, &v); i++) {
      if (i == 0)
        spa_pod_builder_push_prop (&d->b,
                                   SPA_FORMAT_AUDIO_rate,
                                   get_range_type (value));

      spa_pod_builder_int (&d->b, v);
    }
    prop = spa_pod_builder_pop(&d->b);
    if (i > 1)
      prop->body.flags |= SPA_POD_PROP_FLAG_UNSET;
  }
  value = gst_structure_get_value (d->cs, "channels");
  if (value) {
    int v;
    for (i = 0; get_nth_int (value, i, &v); i++) {
      if (i == 0)
        spa_pod_builder_push_prop (&d->b,
                                   SPA_FORMAT_AUDIO_channels,
                                   get_range_type (value));

      spa_pod_builder_int (&d->b, v);
    }
    prop = spa_pod_builder_pop(&d->b);
    if (i > 1)
      prop->body.flags |= SPA_POD_PROP_FLAG_UNSET;
  }
  return TRUE;
}

static uint32_t
write_pod (struct spa_pod_builder *b, const void *data, uint32_t size)
{
  uint32_t ref = b->state.offset;

  if (b->size <= ref) {
    b->size = SPA_ROUND_UP_N (ref + size, 512);
    b->data = realloc (b->data, b->size);
    if (b->data == NULL)
	    return -1;
  }
  memcpy (b->data + ref, data, size);
  return ref;
}

static struct spa_pod *
convert_1 (ConvertData *d)
{
  if (!(d->type = find_media_types (gst_structure_get_name (d->cs))))
    return NULL;

  d->b.write = write_pod;

  spa_pod_builder_push_object (&d->b, d->id, SPA_ID_OBJECT_Format);
  spa_pod_builder_id(&d->b, d->type->media_type);
  spa_pod_builder_id(&d->b, d->type->media_subtype);

  if (d->type->media_type == SPA_MEDIA_TYPE_video)
    handle_video_fields (d);
  else if (d->type->media_type == SPA_MEDIA_TYPE_audio)
    handle_audio_fields (d);

  spa_pod_builder_pop (&d->b);

  return SPA_MEMBER (d->b.data, 0, struct spa_pod);
}

struct spa_pod *
gst_caps_to_format (GstCaps *caps, guint index, uint32_t id)
{
  ConvertData d;
  struct spa_pod *res;

  g_return_val_if_fail (GST_IS_CAPS (caps), NULL);
  g_return_val_if_fail (gst_caps_is_fixed (caps), NULL);

  spa_zero (d);
  d.cf = gst_caps_get_features (caps, index);
  d.cs = gst_caps_get_structure (caps, index);
  d.id = id;

  res = convert_1 (&d);

  return res;
}

static gboolean
foreach_func (GstCapsFeatures *features,
              GstStructure    *structure,
              ConvertData     *d)
{
  struct spa_pod *fmt;

  spa_zero(d->b);
  d->cf = features;
  d->cs = structure;

  if ((fmt = convert_1 (d)))
    g_ptr_array_insert (d->array, -1, fmt);

  return TRUE;
}


GPtrArray *
gst_caps_to_format_all (GstCaps *caps, uint32_t id)
{
  ConvertData d;

  spa_zero (d);
  d.id = id;
  d.array = g_ptr_array_new_full (gst_caps_get_size (caps), (GDestroyNotify)g_free);

  gst_caps_foreach (caps, (GstCapsForeachFunc) foreach_func, &d);

  return d.array;
}

typedef const char *(*id_to_string_func)(uint32_t id);

static const char *video_id_to_string(uint32_t id)
{
  int idx;
  if ((idx = find_index(video_format_map, SPA_N_ELEMENTS(video_format_map), id)) == -1)
    return NULL;
  return gst_video_format_to_string(idx);
}

static const char *audio_id_to_string(uint32_t id)
{
  int idx;
  if ((idx = find_index(audio_format_map, SPA_N_ELEMENTS(audio_format_map), id)) == -1)
    return NULL;
  return gst_audio_format_to_string(idx);
}

static void
handle_id_prop (struct spa_pod_prop *prop, const char *key, id_to_string_func func, GstCaps *res)
{
  const char * str;
  uint32_t *id = SPA_POD_CONTENTS (struct spa_pod_prop, prop);
  uint32_t i, n_items = SPA_POD_PROP_N_VALUES (prop);
  uint32_t flags;

  flags = prop->body.flags;
  if (!(flags & SPA_POD_PROP_FLAG_UNSET))
    flags &= ~SPA_POD_PROP_RANGE_MASK;

  switch (flags & SPA_POD_PROP_RANGE_MASK) {
    case SPA_POD_PROP_RANGE_NONE:
      if (!(str = func(id[0])))
        return;
      gst_caps_set_simple (res, key, G_TYPE_STRING, rindex (str, ':') + 1, NULL);
      break;
    case SPA_POD_PROP_RANGE_ENUM:
    {
      GValue list = { 0 }, v = { 0 };

      g_value_init (&list, GST_TYPE_LIST);
      for (i = 1; i < n_items; i++) {
        if (!(str = func(id[i])))
          continue;

        g_value_init (&v, G_TYPE_STRING);
        g_value_set_string (&v, rindex (str, ':') + 1);
        gst_value_list_append_and_take_value (&list, &v);
      }
      gst_caps_set_value (res, key, &list);
      g_value_unset (&list);
      break;
    }
    default:
      break;
  }
}

static void
handle_int_prop (struct spa_pod_prop *prop, const char *key, GstCaps *res)
{
  uint32_t *val = SPA_POD_CONTENTS (struct spa_pod_prop, prop);
  uint32_t i, n_items = SPA_POD_PROP_N_VALUES (prop);
  uint32_t flags;

  flags = prop->body.flags;
  if (!(flags & SPA_POD_PROP_FLAG_UNSET))
    flags &= ~SPA_POD_PROP_RANGE_MASK;

  switch (flags & SPA_POD_PROP_RANGE_MASK) {
    case SPA_POD_PROP_RANGE_NONE:
      gst_caps_set_simple (res, key, G_TYPE_INT, val[0], NULL);
      break;
    case SPA_POD_PROP_RANGE_MIN_MAX:
    case SPA_POD_PROP_RANGE_STEP:
    {
      if (n_items < 3)
        return;
      gst_caps_set_simple (res, key, GST_TYPE_INT_RANGE, val[1], val[2], NULL);
      break;
    }
    case SPA_POD_PROP_RANGE_ENUM:
    {
      GValue list = { 0 }, v = { 0 };

      g_value_init (&list, GST_TYPE_LIST);
      for (i = 1; i < n_items; i++) {
        g_value_init (&v, G_TYPE_INT);
        g_value_set_int (&v, val[i]);
        gst_value_list_append_and_take_value (&list, &v);
      }
      gst_caps_set_value (res, key, &list);
      g_value_unset (&list);
      break;
    }
    default:
      break;
  }
}

static void
handle_rect_prop (struct spa_pod_prop *prop, const char *width, const char *height, GstCaps *res)
{
  struct spa_rectangle *rect = SPA_POD_CONTENTS (struct spa_pod_prop, prop);
  uint32_t i, n_items = SPA_POD_PROP_N_VALUES (prop);
  uint32_t flags;

  flags = prop->body.flags;
  if (!(flags & SPA_POD_PROP_FLAG_UNSET))
    flags &= ~SPA_POD_PROP_RANGE_MASK;

  switch (flags & SPA_POD_PROP_RANGE_MASK) {
    case SPA_POD_PROP_RANGE_NONE:
      gst_caps_set_simple (res, width, G_TYPE_INT, rect[0].width,
                                height, G_TYPE_INT, rect[0].height, NULL);
      break;
    case SPA_POD_PROP_RANGE_MIN_MAX:
    case SPA_POD_PROP_RANGE_STEP:
    {
      if (n_items < 3)
        return;
      gst_caps_set_simple (res, width, GST_TYPE_INT_RANGE, rect[1].width, rect[2].width,
                                height, GST_TYPE_INT_RANGE, rect[1].height, rect[2].height, NULL);
      break;
    }
    case SPA_POD_PROP_RANGE_ENUM:
    {
      GValue l1 = { 0 }, l2 = { 0 }, v1 = { 0 }, v2 = { 0 };

      g_value_init (&l1, GST_TYPE_LIST);
      g_value_init (&l2, GST_TYPE_LIST);
      for (i = 1; i < n_items; i++) {
        g_value_init (&v1, G_TYPE_INT);
        g_value_set_int (&v1, rect[i].width);
        gst_value_list_append_and_take_value (&l1, &v1);

        g_value_init (&v2, G_TYPE_INT);
        g_value_set_int (&v2, rect[i].height);
        gst_value_list_append_and_take_value (&l2, &v2);
      }
      gst_caps_set_value (res, width, &l1);
      gst_caps_set_value (res, height, &l2);
      g_value_unset (&l1);
      g_value_unset (&l2);
      break;
    }
    default:
      break;
  }
}

static void
handle_fraction_prop (struct spa_pod_prop *prop, const char *key, GstCaps *res)
{
  struct spa_fraction *fract = SPA_POD_CONTENTS (struct spa_pod_prop, prop);
  uint32_t i, n_items = SPA_POD_PROP_N_VALUES (prop);
  uint32_t flags;

  flags = prop->body.flags;
  if (!(flags & SPA_POD_PROP_FLAG_UNSET))
    flags &= ~SPA_POD_PROP_RANGE_MASK;

  switch (flags & SPA_POD_PROP_RANGE_MASK) {
    case SPA_POD_PROP_RANGE_NONE:
      gst_caps_set_simple (res, key, GST_TYPE_FRACTION, fract[0].num, fract[0].denom, NULL);
      break;
    case SPA_POD_PROP_RANGE_MIN_MAX:
    case SPA_POD_PROP_RANGE_STEP:
    {
      if (n_items < 3)
        return;
      gst_caps_set_simple (res, key, GST_TYPE_FRACTION_RANGE, fract[1].num, fract[1].denom,
                                                              fract[2].num, fract[2].denom, NULL);
      break;
    }
    case SPA_POD_PROP_RANGE_ENUM:
    {
      GValue l1 = { 0 }, v1 = { 0 };

      g_value_init (&l1, GST_TYPE_LIST);
      for (i = 1; i < n_items; i++) {
        g_value_init (&v1, GST_TYPE_FRACTION);
        gst_value_set_fraction (&v1, fract[i].num, fract[i].denom);
        gst_value_list_append_and_take_value (&l1, &v1);
      }
      gst_caps_set_value (res, key, &l1);
      g_value_unset (&l1);
      break;
    }
    default:
      break;
  }
}
GstCaps *
gst_caps_from_format (const struct spa_pod *format)
{
  GstCaps *res = NULL;
  uint32_t media_type, media_subtype;
  struct spa_pod_prop *prop;

  spa_pod_object_parse(format, "I", &media_type,
			       "I", &media_subtype);

  if (media_type == SPA_MEDIA_TYPE_video) {
    if (media_subtype == SPA_MEDIA_SUBTYPE_raw) {
      res = gst_caps_new_empty_simple ("video/x-raw");
      if ((prop = spa_pod_find_prop (format, SPA_FORMAT_VIDEO_format))) {
        handle_id_prop (prop, "format", video_id_to_string, res);
      }
    }
    else if (media_subtype == SPA_MEDIA_SUBTYPE_mjpg) {
      res = gst_caps_new_empty_simple ("image/jpeg");
    }
    else if (media_subtype == SPA_MEDIA_SUBTYPE_h264) {
      res = gst_caps_new_simple ("video/x-h264",
          "stream-format", G_TYPE_STRING, "byte-stream",
          "alignment", G_TYPE_STRING, "au",
          NULL);
    }
    if ((prop = spa_pod_find_prop (format, SPA_FORMAT_VIDEO_size))) {
      handle_rect_prop (prop, "width", "height", res);
    }
    if ((prop = spa_pod_find_prop (format, SPA_FORMAT_VIDEO_framerate))) {
      handle_fraction_prop (prop, "framerate", res);
    }
    if ((prop = spa_pod_find_prop (format, SPA_FORMAT_VIDEO_maxFramerate))) {
      handle_fraction_prop (prop, "max-framerate", res);
    }
  } else if (media_type == SPA_MEDIA_TYPE_audio) {
    if (media_subtype == SPA_MEDIA_SUBTYPE_raw) {
      res = gst_caps_new_simple ("audio/x-raw",
          "layout", G_TYPE_STRING, "interleaved",
          NULL);
      if ((prop = spa_pod_find_prop (format, SPA_FORMAT_AUDIO_format))) {
        handle_id_prop (prop, "format", audio_id_to_string, res);
      }
      if ((prop = spa_pod_find_prop (format, SPA_FORMAT_AUDIO_rate))) {
        handle_int_prop (prop, "rate", res);
      }
      if ((prop = spa_pod_find_prop (format, SPA_FORMAT_AUDIO_channels))) {
        handle_int_prop (prop, "channels", res);
      }
    }
    else if (media_subtype == SPA_MEDIA_SUBTYPE_aac) {
    }
  }
  return res;
}
