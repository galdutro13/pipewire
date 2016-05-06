/* GStreamer
 * Copyright (C) <2015> Wim Taymans <wim.taymans@gmail.com>
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

#ifndef __GST_PINOS_PROVIDE_H__
#define __GST_PINOS_PROVIDE_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include <client/pinos.h>

G_BEGIN_DECLS

#define GST_TYPE_PINOS_PROVIDE \
  (gst_pinos_provide_get_type())
#define GST_PINOS_PROVIDE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PINOS_PROVIDE,GstPinosProvide))
#define GST_PINOS_PROVIDE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PINOS_PROVIDE,GstPinosProvideClass))
#define GST_IS_PINOS_PROVIDE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PINOS_PROVIDE))
#define GST_IS_PINOS_PROVIDE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PINOS_PROVIDE))
#define GST_PINOS_PROVIDE_CAST(obj) \
  ((GstPinosProvide *) (obj))

typedef struct _GstPinosProvide GstPinosProvide;
typedef struct _GstPinosProvideClass GstPinosProvideClass;

/**
 * GstPinosProvide:
 *
 * Opaque data structure.
 */
struct _GstPinosProvide {
  GstBaseSink element;

  /*< private >*/
  gchar *client_name;

  /* video state */
  gboolean negotiated;

  GMainContext *context;
  PinosMainLoop *loop;
  PinosContext *ctx;
  PinosStream *stream;
  GstAllocator *allocator;
  GstStructure *properties;

  guint32 id_counter;
  GHashTable *fdids;
};

struct _GstPinosProvideClass {
  GstBaseSinkClass parent_class;
};

GType gst_pinos_provide_get_type (void);

G_END_DECLS

#endif /* __GST_PINOS_PROVIDE_H__ */
