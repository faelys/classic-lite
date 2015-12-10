/*
 * Copyright (c) 2015, Natacha Porté
 * Face design by Łukasz Zalewski
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <pebble.h>

/**********************
 * CONFIGURABLE STATE *
 **********************/

static GColor background_color;
static GColor battery_color;
static GColor bluetooth_color;
static GColor hand_color;
static GColor hour_mark_color;
static GColor inner_rectangle_color;
static GColor minute_mark_color;
static GColor text_color;
static const char *text_font = FONT_KEY_GOTHIC_14;
static char text_format[32] = "%a %d";
static bool bluetooth_vibration = true;
static uint8_t show_battery_icon_below = 100;
#define PERSIST_BUFFER_SIZE 43

#ifdef PBL_SDK_3
#define READ_COLOR(color, byte) do { (color).argb = (byte); } while (0)
#define SAVE_COLOR(byte, color) do { (byte) = (color).argb; } while (0)
#elif PBL_SDK_2
#define READ_COLOR(color, byte) do { (color) = (byte); } while (0)
#define SAVE_COLOR(byte, color) do { (byte) = (color); } while (0)
#endif

static void
read_config(void) {
	uint8_t buffer[PERSIST_BUFFER_SIZE + 1];
	int i;

	i = persist_read_data(1, buffer, sizeof buffer);

	if (i == E_DOES_NOT_EXIST) return;

	if (i != PERSIST_BUFFER_SIZE) {
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "unexpected persist buffer size %d", i);
		return;
	}

	if (buffer[0] != 1) {
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "unknown configuration version %u", (unsigned)(buffer[0]));
		return;
	}

	READ_COLOR(background_color,      buffer[1]);
	READ_COLOR(battery_color,         buffer[2]);
	READ_COLOR(bluetooth_color,       buffer[3]);
	READ_COLOR(hand_color,            buffer[4]);
	READ_COLOR(hour_mark_color,       buffer[5]);
	READ_COLOR(inner_rectangle_color, buffer[6]);
	READ_COLOR(minute_mark_color,     buffer[7]);
	READ_COLOR(text_color,            buffer[8]);

	bluetooth_vibration = (buffer[9] != 0);
	show_battery_icon_below = buffer[10];

	memcpy(text_format, buffer + 11, sizeof text_format);
}

static void
write_config(void) {
	uint8_t buffer[PERSIST_BUFFER_SIZE];
	int i;

	buffer[0] = 1;
	SAVE_COLOR(buffer[1], background_color);
	SAVE_COLOR(buffer[2], battery_color);
	SAVE_COLOR(buffer[3], bluetooth_color);
	SAVE_COLOR(buffer[4], hand_color);
	SAVE_COLOR(buffer[5], hour_mark_color);
	SAVE_COLOR(buffer[6], inner_rectangle_color);
	SAVE_COLOR(buffer[7], minute_mark_color);
	SAVE_COLOR(buffer[8], text_color);

	buffer[9] = bluetooth_vibration ? 1 : 0;
	buffer[10] = show_battery_icon_below;

	memcpy(buffer + 11, text_format, sizeof text_format);

	i = persist_write_data(1, buffer, sizeof buffer);

	if (i < 0 || (size_t)i != sizeof buffer) {
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "error while writing to persistent storage (%d)", i);
	}
}

static GColor
color_from_tuple(Tuple *tuple) {
	uint32_t value = 0;

	if (!tuple) return GColorClear;

	switch (tuple->type) {
	    case TUPLE_UINT:
		switch (tuple->length) {
		    case 1:
			value = tuple->value->uint8;
			break;
		    case 2:
			value = tuple->value->uint16;
			break;
		    case 4:
			value = tuple->value->uint32;
			break;
		    default:
			APP_LOG(APP_LOG_LEVEL_ERROR,
			    "bad uint length %u in color",
			    (unsigned)tuple->length);
			return GColorClear;
		}
		break;

	    case TUPLE_INT:
		switch (tuple->length) {
		    case 1:
			if (tuple->value->int8 >= 0)
				value = tuple->value->int8;
			break;
		    case 2:
			if (tuple->value->int16 >= 0)
				value = tuple->value->int16;
			break;
		    case 4:
			if (tuple->value->int32 >= 0)
				value = tuple->value->int32;
			break;
		    default:
			APP_LOG(APP_LOG_LEVEL_ERROR,
			    "bad int length %u in color",
			    (unsigned)tuple->length);
			return GColorClear;
		}
		break;

	    default:
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "bad type %d for color",
		    (int)tuple->type);
		return GColorClear;
	}

#ifdef PBL_SDK_3
	return GColorFromHEX(value);
#elif PBL_SDK_2
	return (value & 0x808080) ? GColorWhite : GColorBlack;
#endif
}

/*****************
 * HELPER MACROS *
 *****************/

#define QUAD_PATH_POINTS(up, left, down, right) {\
	4, \
	(GPoint []) { \
		{ 0, (up) }, \
		{ (left), 0 }, \
		{ 0, (down) }, \
		{ (right), 0 } } }

#ifdef PBL_SDK_3
#define INSET_RECT(output, input, amount) do { \
	(output) = grect_inset((input), GEdgeInsets((amount))); \
	} while(0)
#else
#define INSET_RECT(output, input, amount) do { \
	(output).origin.x = (input).origin.x + (amount); \
	(output).origin.y = (input).origin.y + (amount); \
	(output).size.w = (input).size.w - 2 * (amount); \
	(output).size.h = (input).size.h - 2 * (amount); \
	} while(0)
#endif

/**********************
 * DISPLAY PRIMITIVES *
 **********************/

#ifdef PBL_RECT
static const GPathInfo minute_hand_path_points
	= QUAD_PATH_POINTS(15, 6, -72, -6);
static const GPathInfo hour_hand_path_points
	= QUAD_PATH_POINTS(15, 7, -50, -7);
#else
static const GPathInfo minute_hand_path_points
	= QUAD_PATH_POINTS(17, 7, -83, -7);
static const GPathInfo hour_hand_path_points
	= QUAD_PATH_POINTS(17, 8, -58, -8);
#endif
static const GPathInfo bluetooth_logo_points = { 7, (GPoint[]) {
	{ -3, -3 },
	{  3,  3 },
	{  0,  6 },
	{  0, -6 },
	{  3, -3 },
	{ -3,  3 },
	{  0,  0 } } };
static const GPathInfo bluetooth_frame_points = { 3, (GPoint[]) {
	{ -13,   9 },
	{   0, -14 },
	{  13,   9 } } };

static struct tm tm_now;
static bool bluetooth_connected = 0;
static Window *window;
static Layer *background_layer;
static Layer *hand_layer;
static Layer *icon_layer;
static TextLayer *text_layer;
static GPath *bluetooth_frame;
static GPath *bluetooth_logo;
static GPath *hour_hand_path;
static GPath *minute_hand_path;
static GPoint center;
static char text_buffer[64];
static uint8_t current_battery = 100;
#define has_battery (current_battery > show_battery_icon_below)

#ifdef PBL_RECT
static void
point_at_angle(GRect *rect, int32_t angle, GPoint *output, int *horizontal) {
	int32_t sin_value = sin_lookup(angle);
	int32_t cos_value = cos_lookup(angle);
	int32_t abs_sin = abs(sin_value);
	int32_t abs_cos = abs(cos_value);
	int32_t width = rect->size.w - 1;
	int32_t height = rect->size.h - 1;

	*horizontal = (height * abs_sin < width * abs_cos);

	if (*horizontal) {
		output->x = (height * sin_value / abs_cos + width) / 2;
		output->y = (cos_value > 0) ? 0 : height;
	}
	else {
		output->x = (sin_value > 0) ? width : 0;
		output->y = (height - width * cos_value / abs_sin) / 2;
	}
	output->x += rect->origin.x;
	output->y += rect->origin.y;
}

static void
background_layer_draw(Layer *layer, GContext *ctx) {
	int horiz, i;
	GRect bounds = layer_get_bounds(layer);
	GRect rect, rect2;
	GPoint pt1, pt2;

	(void)layer;

	graphics_context_set_stroke_color(ctx, minute_mark_color);
	INSET_RECT(rect, bounds, 5);
	for (i = 0; i < 60; i += 1) {
		point_at_angle(&rect, TRIG_MAX_ANGLE * i / 60, &pt1, &horiz);
		pt2.x = pt1.x + horiz;
		pt2.y = pt1.y + (1 - horiz);
		graphics_draw_line(ctx, pt1, pt2);
	}

#ifndef PBL_BW
	graphics_context_set_stroke_width(ctx, 3);
#endif

	graphics_context_set_stroke_color(ctx, hour_mark_color);
	INSET_RECT(rect,  bounds, 11);
	INSET_RECT(rect2, bounds, 22);
	for (i = 0; i < 12; i += 1) {
		point_at_angle(&rect, TRIG_MAX_ANGLE * i / 12, &pt1, &horiz);
		point_at_angle(&rect2, TRIG_MAX_ANGLE * i / 12, &pt2, &horiz);

		graphics_draw_line(ctx, pt1, pt2);

#ifdef PBL_BW
		pt1.x += horiz;        pt2.x += horiz;
		pt1.y += 1 - horiz;    pt2.y += 1 - horiz;
		graphics_draw_line(ctx, pt1, pt2);

		pt1.x -= 2 * horiz;        pt2.x -= 2 * horiz;
		pt1.y -= 2 * (1 - horiz);  pt2.y -= 2 * (1 - horiz);
		graphics_draw_line(ctx, pt1, pt2);
#endif
	}

#ifndef PBL_BW
	graphics_context_set_stroke_width(ctx, 1);
#endif

	INSET_RECT(rect, bounds, 35);
	graphics_context_set_stroke_color(ctx, inner_rectangle_color);
#ifdef PBL_BW
	pt1.y = rect.origin.y;
	pt2.y = rect.origin.y + rect.size.h - 1;
	for (i = rect.origin.x +2; i < rect.origin.x + rect.size.w; i += 3) {
		pt1.x = pt2.x = i;
		graphics_draw_pixel(ctx, pt1);
		graphics_draw_pixel(ctx, pt2);
	}

	pt1.x = rect.origin.x;
	pt2.x = rect.origin.x + rect.size.w - 1;
	for (i = rect.origin.y +2; i < rect.origin.y + rect.size.h; i += 3) {
		pt1.y = pt2.y = i;
		graphics_draw_pixel(ctx, pt1);
		graphics_draw_pixel(ctx, pt2);
	}
#else
	graphics_draw_rect(ctx, rect);
#endif
}
#else
static void
background_layer_draw(Layer *layer, GContext *ctx) {
	const GRect bounds = layer_get_bounds(layer);
	const GPoint center = grect_center_point(&bounds);
	const int32_t radius = (bounds.size.w + bounds.size.h) / 4;
	const int32_t angle_delta = TRIG_MAX_ANGLE / (6 * radius);
	int32_t angle, x, y;
	GRect rect;
	GPoint pt1, pt2;
	int i;

	(void)layer;

	graphics_context_set_stroke_color(ctx, minute_mark_color);
	INSET_RECT(rect, bounds, 5);
	for (i = 0; i < 60; i += 1) {
		angle = TRIG_MAX_ANGLE * i / 60;
		graphics_draw_arc(ctx, rect, GOvalScaleModeFillCircle,
		    angle - angle_delta, angle + angle_delta);
	}

	graphics_context_set_stroke_width(ctx, 3);
	graphics_context_set_stroke_color(ctx, hour_mark_color);
	for (i = 0; i < 12; i += 1) {
		angle = TRIG_MAX_ANGLE * i / 12;
		x = sin_lookup(angle);
		y = -cos_lookup(angle);
		pt1.x = center.x + (radius - 11) * x / TRIG_MAX_RATIO;
		pt1.y = center.y + (radius - 11) * y / TRIG_MAX_RATIO;
		pt2.x = center.x + (radius - 22) * x / TRIG_MAX_RATIO;
		pt2.y = center.y + (radius - 22) * y / TRIG_MAX_RATIO;
		graphics_draw_line(ctx, pt1, pt2);
	}

	graphics_context_set_stroke_width(ctx, 1);
	graphics_context_set_stroke_color(ctx, inner_rectangle_color);
	graphics_draw_circle(ctx, center, radius - 35);
}
#endif

static void
hand_layer_draw(Layer *layer, GContext *ctx) {
	(void)layer;

	graphics_context_set_fill_color(ctx, hand_color);
	graphics_context_set_stroke_color(ctx, background_color);

	gpath_rotate_to(minute_hand_path, TRIG_MAX_ANGLE * tm_now.tm_min / 60);
	gpath_draw_filled(ctx, minute_hand_path);
	gpath_draw_outline(ctx, minute_hand_path);

	gpath_rotate_to(hour_hand_path,
	    TRIG_MAX_ANGLE * (tm_now.tm_hour * 60 + tm_now.tm_min) / 720);
	gpath_draw_filled(ctx, hour_hand_path);
	gpath_draw_outline(ctx, hour_hand_path);

	graphics_context_set_fill_color(ctx, background_color);
	graphics_fill_circle(ctx, center, 2);
	graphics_context_set_fill_color(ctx, hand_color);
	graphics_fill_circle(ctx, center, 1);
}

static void
icon_layer_draw(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds);
	GPoint pt;

	if (!bluetooth_connected) {
		pt.x = center.x;
		pt.y = center.y + (has_battery ? +1 : -2);
		gpath_move_to(bluetooth_frame, pt);
		gpath_move_to(bluetooth_logo, pt);
		graphics_context_set_stroke_color(ctx, bluetooth_color);
		gpath_draw_outline(ctx, bluetooth_frame);
#ifdef PBL_SDK_3
		gpath_draw_outline_open(ctx, bluetooth_logo);
#else
		gpath_draw_outline(ctx, bluetooth_logo);
#endif
	}

	if (!has_battery) {
		pt.x = center.x - 11;
		pt.y = center.y
		    + (bluetooth_connected ? 0 : PBL_IF_RECT_ELSE(9, 11));
		graphics_context_set_fill_color(ctx, battery_color);
		graphics_fill_rect(ctx,
		    GRect(pt.x, pt.y, 22, 7),
		    0, GCornerNone);
		graphics_fill_rect(ctx,
		    GRect(pt.x + 22, pt.y + 2, 2, 3),
		    0, GCornerNone);
		graphics_context_set_fill_color(ctx, background_color);
		if (current_battery >= 5)
			graphics_fill_rect(ctx,
			    GRect(pt.x + 1 + current_battery / 5, pt.y + 1,
			    20 - current_battery / 5, 5),
			    0, GCornerNone);
	}
}

static void
update_text_layer(struct tm *time) {
	strftime(text_buffer, sizeof text_buffer, text_format, time);
	text_layer_set_text(text_layer, text_buffer);
}

/********************
 * SERVICE HANDLERS *
 ********************/

#ifdef PBL_ROUND
static void
app_did_focus(bool in_focus) {
	if (in_focus) {
		layer_mark_dirty(background_layer);
		layer_mark_dirty(hand_layer);
		layer_mark_dirty(icon_layer);
		layer_mark_dirty(text_layer_get_layer(text_layer));
	}
}
#endif

static void
battery_handler(BatteryChargeState charge) {
	if (current_battery == charge.charge_percent) return;
	current_battery = charge.charge_percent;
	layer_set_hidden(icon_layer, bluetooth_connected && has_battery);
	if (!has_battery) layer_mark_dirty(icon_layer);
}

static void
bluetooth_handler(bool connected) {
	bluetooth_connected = connected;
	layer_set_hidden(icon_layer, connected && has_battery);
	layer_mark_dirty(icon_layer);

	if (bluetooth_vibration && !connected) vibes_long_pulse();
}

static void
inbox_received_handler(DictionaryIterator *iterator, void *context) {
	Tuple *tuple;

	(void)context;

	for (tuple = dict_read_first(iterator);
	    tuple;
	    tuple = dict_read_next(iterator)) {
		switch (tuple->key) {
		    case 1:
			background_color = color_from_tuple(tuple);
			window_set_background_color(window, background_color);
			text_layer_set_background_color(text_layer,
			    background_color);
			break;
		    case 2:
			battery_color = color_from_tuple(tuple);
			layer_mark_dirty(icon_layer);
			break;
		    case 3:
			bluetooth_color = color_from_tuple(tuple);
			layer_mark_dirty(icon_layer);
			break;
		    case 4:
			hand_color = color_from_tuple(tuple);
			layer_mark_dirty(hand_layer);
			break;
		    case 5:
			hour_mark_color = color_from_tuple(tuple);
			layer_mark_dirty(background_layer);
			break;
		    case 6:
			inner_rectangle_color = color_from_tuple(tuple);
			layer_mark_dirty(background_layer);
			break;
		    case 7:
			minute_mark_color = color_from_tuple(tuple);
			layer_mark_dirty(background_layer);
			break;
		    case 8:
			text_color = color_from_tuple(tuple);
			text_layer_set_text_color(text_layer, text_color);
			break;
		/*  case 9: is reserved for a future color */
		/*  case 10: TODO: text_font */
		    case 11:
			if (tuple->type == TUPLE_CSTRING) {
				strncpy(text_format, tuple->value->cstring,
				   sizeof text_format);
				update_text_layer(&tm_now);
			} else
				APP_LOG(APP_LOG_LEVEL_ERROR,
				    "bad type %d for text_format entry",
				    (int)tuple->type);
			break;
		    case 12:
			if (tuple->type == TUPLE_INT
			    || tuple->type == TUPLE_UINT)
				bluetooth_vibration
				    = tuple->value->data[0] != 0;
			else
				APP_LOG(APP_LOG_LEVEL_ERROR,
				    "bad type %d for bluetooth_vibration entry",
				    (int)tuple->type);
			break;
		    case 13:
			if (tuple->type == TUPLE_INT)
				show_battery_icon_below
				    = (tuple->value->int8 < 0) ? 0
				    : tuple->value->int8;
			else if (tuple->type == TUPLE_UINT)
				show_battery_icon_below = tuple->value->uint8;
			else
				APP_LOG(APP_LOG_LEVEL_ERROR, "bad type %d for "
				    "show_battery_icon_below entry",
				    (int)tuple->type);
			layer_set_hidden(icon_layer,
			    bluetooth_connected && has_battery);
			break;
		    default:
			APP_LOG(APP_LOG_LEVEL_ERROR,
			    "unknown configuration key %lu",
			    (unsigned long)tuple->key);
		}
	}

	write_config();
}

static void
tick_handler(struct tm* tick_time, TimeUnits units_changed) {
	if (tm_now.tm_mday != tick_time->tm_mday) update_text_layer(tick_time);
	tm_now = *tick_time;
	layer_mark_dirty(hand_layer);
}

/***********************************
 * INITIALIZATION AND FINALIZATION *
 ***********************************/

static void
window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	window_set_background_color(window, background_color);

	center = grect_center_point(&bounds);
	gpath_move_to(hour_hand_path, center);
	gpath_move_to(minute_hand_path, center);

	background_layer = layer_create(bounds);
	layer_set_update_proc(background_layer, &background_layer_draw);
	layer_add_child(window_layer, background_layer);

	text_layer = text_layer_create(GRect(
	    bounds.origin.x + (bounds.size.w - 72) / 2,
	    bounds.origin.y + 36 + PBL_IF_RECT_ELSE(5, 15),
	    72, 23));
	text_layer_set_text_color(text_layer, text_color);
	text_layer_set_background_color(text_layer, background_color);
	text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
	text_layer_set_font(text_layer, fonts_get_system_font(text_font));
	layer_add_child(window_layer, text_layer_get_layer(text_layer));
	update_text_layer(&tm_now);

	icon_layer = layer_create(GRect(bounds.origin.x
	    + (bounds.size.w - 33) / 2, PBL_IF_RECT_ELSE(97, 105), 33, 36));
	layer_set_update_proc(icon_layer, &icon_layer_draw);
	layer_set_hidden(icon_layer, bluetooth_connected && has_battery);
	layer_add_child(window_layer, icon_layer);

	hand_layer = layer_create(bounds);
	layer_set_update_proc(hand_layer, &hand_layer_draw);
	layer_add_child(window_layer, hand_layer);

#ifdef PBL_ROUND
	app_focus_service_subscribe_handlers((AppFocusHandlers){
	    .will_focus = 0,
	    .did_focus = &app_did_focus,
	});
#endif
}

static void
window_unload(Window *window) {
	app_focus_service_unsubscribe();
	layer_destroy(background_layer);
	layer_destroy(hand_layer);
	layer_destroy(icon_layer);
	text_layer_destroy(text_layer);
}

static void
init(void) {
	time_t current_time = time(0);
	tm_now = *localtime(&current_time);

	background_color = GColorWhite;
	bluetooth_color = GColorBlack;
	hand_color = GColorBlack;
	hour_mark_color = GColorBlack;
	minute_mark_color = GColorBlack;
	text_color = GColorBlack;

#ifdef PBL_BW
	battery_color = GColorBlack;
	inner_rectangle_color = GColorBlack;
#else
	battery_color = GColorDarkGray;
	inner_rectangle_color = GColorLightGray;
#endif

	bluetooth_connected = connection_service_peek_pebble_app_connection();
	current_battery = battery_state_service_peek().charge_percent;
	read_config();

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	bluetooth_frame = gpath_create(&bluetooth_frame_points);
	bluetooth_logo = gpath_create(&bluetooth_logo_points);
	hour_hand_path = gpath_create(&hour_hand_path_points);
	minute_hand_path = gpath_create(&minute_hand_path_points);

	battery_state_service_subscribe(&battery_handler);
	connection_service_subscribe(((ConnectionHandlers){
	    .pebble_app_connection_handler = &bluetooth_handler,
	    .pebblekit_connection_handler = 0}));
	tick_timer_service_subscribe(MINUTE_UNIT, &tick_handler);
	window_stack_push(window, true);

	app_message_register_inbox_received(inbox_received_handler);
	app_message_open(app_message_inbox_size_maximum(), 0);
}

static void
deinit(void) {
	battery_state_service_unsubscribe();
	connection_service_unsubscribe();
	tick_timer_service_unsubscribe();
	gpath_destroy(bluetooth_frame);
	gpath_destroy(bluetooth_logo);
	gpath_destroy(hour_hand_path);
	gpath_destroy(minute_hand_path);
	window_destroy(window);
}

int
main(void) {
	init();
	app_event_loop();
	deinit();
}
