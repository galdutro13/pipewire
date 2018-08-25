/* PipeWire
 * Copyright (C) 2017 Wim Taymans <wim.taymans@gmail.com>
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

#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <sys/mman.h>

#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/node/io.h>
#include <spa/debug/format.h>

#include <pipewire/pipewire.h>

#define M_PI_M2 ( M_PI + M_PI )

#define BUFFER_SAMPLES	128

#define DEFAULT_VOLUME 1.0

struct props {
	double volume;
};

static void reset_props(struct props *props)
{
	props->volume = DEFAULT_VOLUME;
}

struct buffer {
	struct spa_buffer *buffer;
	struct spa_list link;
	void *ptr;
	bool mapped;
};

struct data {
	struct props props;

	const char *path;

	struct pw_main_loop *loop;

	struct pw_core *core;

	struct pw_remote *remote;
	struct spa_hook remote_listener;

	struct pw_node *node;
	struct spa_port_info port_info;
	struct spa_dict port_props;
	struct spa_dict_item port_items[1];

	struct spa_node impl_node;
	const struct spa_node_callbacks *callbacks;
	void *callbacks_data;
	struct spa_io_buffers *io;

	struct spa_pod_double *ctrl_volume;

	struct spa_audio_info_raw format;

	struct buffer buffers[32];
	int n_buffers;
	struct spa_list empty;

	double accumulator;
	double volume_accum;
};

static void update_volume(struct data *data)
{
	if (data->ctrl_volume == NULL)
		return;

        data->ctrl_volume->value = (sin(data->volume_accum) / 2.0) + 0.5;
        data->volume_accum += M_PI_M2 / 1000.0;
        if (data->volume_accum >= M_PI_M2)
                data->volume_accum -= M_PI_M2;
}

static int impl_send_command(struct spa_node *node, const struct spa_command *command)
{
	return 0;
}

static int impl_set_callbacks(struct spa_node *node,
			      const struct spa_node_callbacks *callbacks, void *data)
{
	struct data *d = SPA_CONTAINER_OF(node, struct data, impl_node);
	d->callbacks = callbacks;
	d->callbacks_data = data;
	return 0;
}

static int impl_get_n_ports(struct spa_node *node,
			    uint32_t *n_input_ports,
			    uint32_t *max_input_ports,
			    uint32_t *n_output_ports,
			    uint32_t *max_output_ports)
{
	*n_input_ports = *max_input_ports = 0;
	*n_output_ports = *max_output_ports = 1;
	return 0;
}

static int impl_get_port_ids(struct spa_node *node,
                             uint32_t *input_ids,
                             uint32_t n_input_ids,
                             uint32_t *output_ids,
                             uint32_t n_output_ids)
{
	if (n_output_ids > 0)
                output_ids[0] = 0;
	return 0;
}

static int impl_port_set_io(struct spa_node *node, enum spa_direction direction, uint32_t port_id,
			    uint32_t id, void *data, size_t size)
{
	struct data *d = SPA_CONTAINER_OF(node, struct data, impl_node);

	if (id == SPA_IO_Buffers)
		d->io = data;
#if 0
	else if (id == type.io_prop_volume) {
		if (data && size >= sizeof(struct spa_pod_double))
			d->ctrl_volume = data;
		else
			d->ctrl_volume = NULL;
	}
#endif
	else
		return -ENOENT;

	return 0;
}

static int impl_port_get_info(struct spa_node *node, enum spa_direction direction, uint32_t port_id,
			      const struct spa_port_info **info)
{
	struct data *d = SPA_CONTAINER_OF(node, struct data, impl_node);

	d->port_info.flags = SPA_PORT_INFO_FLAG_CAN_USE_BUFFERS;
	d->port_info.rate = 0;
	d->port_info.props = &d->port_props;

	d->port_items[0] = SPA_DICT_ITEM_INIT("port.dsp", "32 bit float mono audio");
	d->port_props = SPA_DICT_INIT(d->port_items, 1);

	*info = &d->port_info;

	return 0;
}

static int port_enum_formats(struct spa_node *node,
			     enum spa_direction direction, uint32_t port_id,
			     uint32_t *index,
			     const struct spa_pod *filter,
			     struct spa_pod **param,
			     struct spa_pod_builder *builder)
{
	if (*index != 0)
		return 0;

	*param = spa_pod_builder_object(builder,
		SPA_PARAM_EnumFormat, SPA_ID_OBJECT_Format,
		"I", SPA_MEDIA_TYPE_audio,
		"I", SPA_MEDIA_SUBTYPE_raw,
		":", SPA_FORMAT_AUDIO_format,   "Ieu", SPA_AUDIO_FORMAT_S16,
			SPA_POD_PROP_ENUM(2, SPA_AUDIO_FORMAT_S16,
					     SPA_AUDIO_FORMAT_F32),
		":", SPA_FORMAT_AUDIO_layout,   "Ieu", SPA_AUDIO_LAYOUT_INTERLEAVED,
			SPA_POD_PROP_ENUM(2, SPA_AUDIO_LAYOUT_INTERLEAVED,
                                             SPA_AUDIO_LAYOUT_NON_INTERLEAVED),
		":", SPA_FORMAT_AUDIO_channels, "iru", 2,
			SPA_POD_PROP_MIN_MAX(1, INT32_MAX),
		":", SPA_FORMAT_AUDIO_rate,     "iru", 44100,
			SPA_POD_PROP_MIN_MAX(1, INT32_MAX));

	(*index)++;

	return 1;
}

static int port_get_format(struct spa_node *node,
			   enum spa_direction direction, uint32_t port_id,
			   uint32_t *index,
			   const struct spa_pod *filter,
			   struct spa_pod **param,
			   struct spa_pod_builder *builder)
{
	struct data *d = SPA_CONTAINER_OF(node, struct data, impl_node);

	if (*index != 0)
		return 0;

	if (d->format.format == 0)
		return 0;

	*param = spa_pod_builder_object(builder,
		SPA_PARAM_Format, SPA_ID_OBJECT_Format,
		"I", SPA_MEDIA_TYPE_audio,
		"I", SPA_MEDIA_SUBTYPE_raw,
		":", SPA_FORMAT_AUDIO_format,   "I", d->format.format,
		":", SPA_FORMAT_AUDIO_layout,   "I", d->format.layout,
		":", SPA_FORMAT_AUDIO_channels, "i", d->format.channels,
		":", SPA_FORMAT_AUDIO_rate,     "i", d->format.rate);

	(*index)++;

	return 1;
}

static int impl_port_enum_params(struct spa_node *node,
				 enum spa_direction direction, uint32_t port_id,
				 uint32_t id, uint32_t *index,
				 const struct spa_pod *filter,
				 struct spa_pod **result,
				 struct spa_pod_builder *builder)
{
	struct spa_pod *param;

	switch (id) {
	case SPA_PARAM_List:
	{
		uint32_t list[] = { SPA_PARAM_EnumFormat,
				    SPA_PARAM_Format,
				    SPA_PARAM_Buffers,
				    SPA_PARAM_Meta,
				    SPA_PARAM_IO };

		if (*index < SPA_N_ELEMENTS(list))
			param = spa_pod_builder_object(builder,
				id, SPA_ID_OBJECT_ParamList,
				":", SPA_PARAM_LIST_id, "I", list[*index]);
		else
			return 0;
		break;
	}
	case SPA_PARAM_EnumFormat:
		return port_enum_formats(node, direction, port_id, index, filter, result, builder);

	case SPA_PARAM_Format:
		return port_get_format(node, direction, port_id, index, filter, result, builder);

	case SPA_PARAM_Buffers:
		if (*index > 0)
			return 0;

		param = spa_pod_builder_object(builder,
			id, SPA_ID_OBJECT_ParamBuffers,
			":", SPA_PARAM_BUFFERS_buffers, "iru", 1,
				SPA_POD_PROP_MIN_MAX(1, 32),
			":", SPA_PARAM_BUFFERS_blocks,  "i",   1,
			":", SPA_PARAM_BUFFERS_size,    "iru", BUFFER_SAMPLES * sizeof(float),
				SPA_POD_PROP_MIN_MAX(32, 4096),
			":", SPA_PARAM_BUFFERS_stride,  "i",   0,
			":", SPA_PARAM_BUFFERS_align,   "i",   16);
		break;
	case SPA_PARAM_Meta:
		switch (*index) {
		case 0:
			param = spa_pod_builder_object(builder,
				id, SPA_ID_OBJECT_ParamMeta,
				":", SPA_PARAM_META_type, "I", SPA_META_Header,
				":", SPA_PARAM_META_size, "i", sizeof(struct spa_meta_header));
			break;
		default:
			return 0;
		}
		break;
	case SPA_PARAM_IO:
		switch (*index) {
		case 0:
			param = spa_pod_builder_object(builder,
				id, SPA_ID_OBJECT_ParamIO,
				":", SPA_PARAM_IO_id, "I", SPA_IO_Buffers,
				":", SPA_PARAM_IO_size, "i", sizeof(struct spa_io_buffers));
			break;
		default:
			return 0;
		}
		break;
#if 0
	else if (id == t->param_io.idPropsOut) {
		struct props *p = &d->props;

		switch (*index) {
		case 0:
			param = spa_pod_builder_object(builder,
				id, t->param_io.Prop,
				":", t->param_io.id, "I", d->type.io_prop_volume,
				":", t->param_io.size, "i", sizeof(struct spa_pod_double),
				":", t->param.propId, "I", d->type.prop_volume,
				":", t->param.propType, "dru", p->volume,
					SPA_POD_PROP_MIN_MAX(0.0, 10.0));
			break;
		default:
			return 0;
		}
	}
#endif
	default:
		return -ENOENT;
	}

	(*index)++;
	*result = param;

	return 1;
}

static int port_set_format(struct spa_node *node,
			   enum spa_direction direction, uint32_t port_id,
			   uint32_t flags, const struct spa_pod *format)
{
	struct data *d = SPA_CONTAINER_OF(node, struct data, impl_node);

	if (format == NULL) {
		d->format.format = 0;
		return 0;
	}

	spa_debug_format(0, NULL, format);

	if (spa_format_audio_raw_parse(format, &d->format) < 0)
		return -EINVAL;

	if (d->format.format != SPA_AUDIO_FORMAT_S16 &&
	    d->format.format != SPA_AUDIO_FORMAT_F32)
		return -EINVAL;

	return 0;
}

static int impl_port_set_param(struct spa_node *node,
			       enum spa_direction direction, uint32_t port_id,
			       uint32_t id, uint32_t flags,
			       const struct spa_pod *param)
{
	if (id == SPA_PARAM_Format) {
		return port_set_format(node, direction, port_id, flags, param);
	}
	else
		return -ENOENT;
}

static int impl_port_use_buffers(struct spa_node *node, enum spa_direction direction, uint32_t port_id,
				 struct spa_buffer **buffers, uint32_t n_buffers)
{
	struct data *d = SPA_CONTAINER_OF(node, struct data, impl_node);
	int i;
	for (i = 0; i < n_buffers; i++) {
		struct buffer *b = &d->buffers[i];
		struct spa_data *datas = buffers[i]->datas;

		if (datas[0].data != NULL) {
			b->ptr = datas[0].data;
			b->mapped = false;
		}
		else if (datas[0].type == SPA_DATA_MemFd ||
			 datas[0].type == SPA_DATA_DmaBuf) {
			b->ptr = mmap(NULL, datas[0].maxsize + datas[0].mapoffset, PROT_WRITE,
				      MAP_SHARED, datas[0].fd, 0);
			if (b->ptr == MAP_FAILED) {
				pw_log_error("failed to buffer mem");
				return -errno;

			}
			b->ptr = SPA_MEMBER(b->ptr, datas[0].mapoffset, void);
			b->mapped = true;
		}
		else {
			pw_log_error("invalid buffer mem");
			return -EINVAL;
		}
		b->buffer = buffers[i];
		pw_log_info("got buffer %d size %d", i, datas[0].maxsize);
		spa_list_append(&d->empty, &b->link);
	}
	d->n_buffers = n_buffers;
	return 0;
}

static inline void reuse_buffer(struct data *d, uint32_t id)
{
	pw_log_trace("export-source %p: recycle buffer %d", d, id);
        spa_list_append(&d->empty, &d->buffers[id].link);
}

static int impl_port_reuse_buffer(struct spa_node *node, uint32_t port_id, uint32_t buffer_id)
{
	struct data *d = SPA_CONTAINER_OF(node, struct data, impl_node);
	reuse_buffer(d, buffer_id);
	return 0;
}

static void fill_f32(struct data *d, void *dest, int avail)
{
	float *dst = dest;
	int n_samples = avail / (sizeof(float) * d->format.channels);
	int i, c;

        for (i = 0; i < n_samples; i++) {
		float val;

                d->accumulator += M_PI_M2 * 440 / d->format.rate;
                if (d->accumulator >= M_PI_M2)
                        d->accumulator -= M_PI_M2;

                val = sin(d->accumulator);

                for (c = 0; c < d->format.channels; c++)
                        *dst++ = val;
        }
}

static void fill_s16(struct data *d, void *dest, int avail)
{
	int16_t *dst = dest;
	int n_samples = avail / (sizeof(int16_t) * d->format.channels);
	int i, c;

        for (i = 0; i < n_samples; i++) {
                int16_t val;

                d->accumulator += M_PI_M2 * 440 / d->format.rate;
                if (d->accumulator >= M_PI_M2)
                        d->accumulator -= M_PI_M2;

                val = (int16_t) (sin(d->accumulator) * 32767.0);

                for (c = 0; c < d->format.channels; c++)
                        *dst++ = val;
        }
}

static int impl_node_process(struct spa_node *node)
{
	struct data *d = SPA_CONTAINER_OF(node, struct data, impl_node);
	struct buffer *b;
	int avail;
        struct spa_io_buffers *io = d->io;
	uint32_t maxsize, index = 0;
	uint32_t filled, offset;
	struct spa_data *od;

	if (io->buffer_id < d->n_buffers) {
		reuse_buffer(d, io->buffer_id);
		io->buffer_id = SPA_ID_INVALID;
	}
	if (spa_list_is_empty(&d->empty)) {
                pw_log_error("export-source %p: out of buffers", d);
                return -EPIPE;
        }
        b = spa_list_first(&d->empty, struct buffer, link);
        spa_list_remove(&b->link);

	od = b->buffer->datas;

	maxsize = od[0].maxsize;

	filled = 0;
	index = 0;
	avail = maxsize - filled;
	offset = index % maxsize;

	if (offset + avail > maxsize)
		avail = maxsize - offset;

	if (d->format.format == SPA_AUDIO_FORMAT_S16)
		fill_s16(d, SPA_MEMBER(b->ptr, offset, void), avail);
	else if (d->format.format == SPA_AUDIO_FORMAT_F32)
		fill_f32(d, SPA_MEMBER(b->ptr, offset, void), avail);

	od[0].chunk->offset = 0;
	od[0].chunk->size = avail;
	od[0].chunk->stride = 0;

	io->buffer_id = b->buffer->id;
	io->status = SPA_STATUS_HAVE_BUFFER;

	update_volume(d);

	return SPA_STATUS_HAVE_BUFFER;
}

static const struct spa_node impl_node = {
	SPA_VERSION_NODE,
	.send_command = impl_send_command,
	.set_callbacks = impl_set_callbacks,
	.get_n_ports = impl_get_n_ports,
	.get_port_ids = impl_get_port_ids,
	.port_set_io = impl_port_set_io,
	.port_get_info = impl_port_get_info,
	.port_enum_params = impl_port_enum_params,
	.port_set_param = impl_port_set_param,
	.port_use_buffers = impl_port_use_buffers,
	.port_reuse_buffer = impl_port_reuse_buffer,
	.process = impl_node_process,
};

static void make_node(struct data *data)
{
	struct pw_properties *props;

	props = pw_properties_new(PW_NODE_PROP_AUTOCONNECT, "1",
				  PW_NODE_PROP_EXCLUSIVE, "0",
				  NULL);
	if (data->path)
		pw_properties_set(props, PW_NODE_PROP_TARGET_NODE, data->path);

	data->node = pw_node_new(data->core, "export-source", props, 0);
	data->impl_node = impl_node;
	pw_node_set_implementation(data->node, &data->impl_node);

	pw_node_register(data->node, NULL, NULL, NULL);
	pw_node_set_active(data->node, true);

	pw_remote_export(data->remote, data->node);
}

static void on_state_changed(void *_data, enum pw_remote_state old,
			     enum pw_remote_state state, const char *error)
{
	struct data *data = _data;

	switch (state) {
	case PW_REMOTE_STATE_ERROR:
		printf("remote error: %s\n", error);
		pw_main_loop_quit(data->loop);
		break;

	case PW_REMOTE_STATE_CONNECTED:
		make_node(data);
		break;

	default:
		printf("remote state: \"%s\"\n", pw_remote_state_as_string(state));
		break;
	}
}

static const struct pw_remote_events remote_events = {
	PW_VERSION_REMOTE_EVENTS,
	.state_changed = on_state_changed,
};

int main(int argc, char *argv[])
{
	struct data data = { 0, };

	pw_init(&argc, &argv);

	data.loop = pw_main_loop_new(NULL);
	data.core = pw_core_new(pw_main_loop_get_loop(data.loop), NULL);
        data.remote = pw_remote_new(data.core, NULL, 0);
	data.path = argc > 1 ? argv[1] : NULL;

	spa_list_init(&data.empty);
	reset_props(&data.props);

	pw_remote_add_listener(data.remote, &data.remote_listener, &remote_events, &data);

        pw_remote_connect(data.remote);

	pw_main_loop_run(data.loop);

	pw_core_destroy(data.core);
	pw_main_loop_destroy(data.loop);

	return 0;
}
