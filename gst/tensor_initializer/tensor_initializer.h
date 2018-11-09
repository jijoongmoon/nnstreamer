/**
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2018 Jijoong Moon <jijoong.moon@samsung.com>
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
 */

/**
 * @file	tensor_initializer.h
 * @date	08 Nov 2018
 * @brief	initialized tensor
 * @see		https://github.com/nnsuite/nnstreamer
 * @author	Jijoong Moon <jijoong.moon@samsung.com>
 * @bug		No known bugs except for NYI items
 */

#ifndef __GST_TENSOR_INITIALIZER_H__
#define __GST_TENSOR_INITIALIZER_H__

#include <gst/gst.h>
#include <tensor_common.h>

G_BEGIN_DECLS

#define GST_TYPE_TENSOR_INITIALIZER \
  (gst_tensor_initializer_get_type())
#define GST_TENSOR_INITIALIZER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TENSOR_INITIALIZER,GstTensorInitializer))
#define GST_TENSOR_INITIALIZER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TENSOR_INITIALIZER,GstTensorInitializerClass))
#define GST_IS_TENSOR_INITIALIZER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TENSOR_INITIALIZER))
#define GST_IS_TENSOR_INITIALIZER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TENSOR_INITIALIZER))
#define GST_TENSOR_INITIALIZER_CAST(obj) ((GstTensorInitializer*) obj)

typedef struct _GstTensorInitializer GstTensorInitializer;
typedef struct _GstTensorInitializerClass GstTensorInitializerClass;

#define GST_TENSOR_INITIALIZER_GET_LOCK(sel) (&((GstTensorInitializer*)(sel))->lock)
#define GST_TENSOR_INITIALIZER_GET_COND(sel) (&((GstTensorInitializer*)(sel))->cond)
#define GST_TENSOR_INITIALIZER_LOCK(sel) (g_mutex_lock (GST_TENSOR_INITIALIZER_GET_LOCK(sel)))
#define GST_TENSOR_INITIALIZER_UNLOCK(sel) (g_mutex_unlock (GST_TENSOR_INITIALIZER_GET_LOCK(sel)))
#define GST_TENSOR_INITIALIZER_WAIT(sel) (g_cond_wait (GST_TENSOR_INITIALIZER_GET_COND(sel), \
			GST_TENSOR_INITIALIZER_GET_LOCK(sel)))
#define GST_TENSOR_INITIALIZER_BROADCAST(sel) (g_cond_broadcast (GST_TENSOR_INITIALIZER_GET_COND(sel)))

/**
 * @brief Internal data structure for tensorscheck instances.
 */
struct _GstTensorInitializer
{
  GstElement element;

  GstPad *active_sinkpad, *srcpad;
  guint n_pads;
  guint padcount;
  GMutex lock;
  GCond cond;
  gboolean eos;
  gboolean eos_sent;
  gboolean flushing;

  gboolean silent;
  gboolean passthrough;
  gboolean have_group_id;

  /* For Tensor */
  GstTensorConfig in_config;
  GstTensorConfig out_config;
};

/**
 * @brief GstTensorInitializerClass inherits GstElementClass
 */
struct _GstTensorInitializerClass
{
  GstElementClass parent_class;
};

/**
 * @brief Get Type function required for gst elements
 */
GType gst_tensor_initializer_get_type (void);

G_END_DECLS

#endif /* __GST_TENSOR_INITIALIZER_H__ */
