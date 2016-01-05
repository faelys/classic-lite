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

#define CACHE_BACKGROUND

/**********************
 * CONFIGURABLE STATE *
 **********************/

static GColor background_color;
static GColor battery_color;
static GColor battery_color2;
static GColor bluetooth_color;
static GColor hour_hand_color;
static GColor minute_hand_color;
static GColor pin_color;
static GColor hour_mark_color;
static GColor inner_rectangle_color;
static GColor minute_mark_color;
static GColor text_color;
static unsigned text_font = 0;
static char text_format[32] = "%a %d";
static bool bluetooth_vibration = true;
static uint8_t show_battery_icon_below = 100;
#define PERSIST_BUFFER_SIZE 47
#define TEXT_FONT_NUMBER 4

static const char *const text_fonts[] = {
	FONT_KEY_GOTHIC_14,
	FONT_KEY_GOTHIC_18,
	FONT_KEY_GOTHIC_24,
	FONT_KEY_GOTHIC_28,
};

static const uint16_t text_offsets[] = {
	36 + PBL_IF_RECT_ELSE(5, 15),
	36 + PBL_IF_RECT_ELSE(2, 12),
	36 + PBL_IF_RECT_ELSE(-1, 8),
	36 + PBL_IF_RECT_ELSE(-2, 7),
};

static const uint16_t text_heights[] = {
	17,
	22,
	29,
	33,
};

#ifdef PBL_SDK_3
#define IS_VISIBLE(color) ((color).argb != background_color.argb)
#define IS_EQUAL(color1, color2) ((color1).argb == (color2).argb)
#define READ_COLOR(color, byte) do { (color).argb = (byte); } while (0)
#define SAVE_COLOR(byte, color) do { (byte) = (color).argb; } while (0)
#elif PBL_SDK_2
#define IS_VISIBLE(color) ((color) != background_color)
#define IS_EQUAL(color1, color2) ((color1) == (color2))
#define READ_COLOR(color, byte) do { (color) = (byte); } while (0)
#define SAVE_COLOR(byte, color) do { (byte) = (color); } while (0)
#endif

static void
read_config(void) {
	uint8_t buffer[PERSIST_BUFFER_SIZE + 1];
	int i;

	i = persist_read_data(1, buffer, sizeof buffer);

	if (i == E_DOES_NOT_EXIST) return;

	if (i < 1) {
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "invalid persist buffer size %d", i);
		return;
	}

	if (buffer[0] < 1) {
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "invalid configuration version %u", (unsigned)(buffer[0]));
		return;
	}

	if (buffer[0] > 3) {
		APP_LOG(APP_LOG_LEVEL_WARNING,
		    "loading data from future version %u, "
		    "data will be lost on the next write",
		    (unsigned)(buffer[0]));
		return;
	}

	if (i < 43) {
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "truncated persistent buffer size at %d, aborting", i);
		return;
	}

	READ_COLOR(background_color,      buffer[1]);
	READ_COLOR(battery_color,         buffer[2]);
	READ_COLOR(bluetooth_color,       buffer[3]);
	READ_COLOR(hour_hand_color,       buffer[4]);
	READ_COLOR(hour_mark_color,       buffer[5]);
	READ_COLOR(inner_rectangle_color, buffer[6]);
	READ_COLOR(minute_mark_color,     buffer[7]);
	READ_COLOR(text_color,            buffer[8]);

	bluetooth_vibration = (buffer[9] != 0);
	show_battery_icon_below = buffer[10];

	memcpy(text_format, buffer + 11, sizeof text_format);

	battery_color2 = battery_color;
	pin_color = minute_hand_color = hour_hand_color;

	if (buffer[0] < 2) return;

	if (i < 45) {
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "truncated persistent buffer (size %d), using only v1",
		    i);
		return;
	}

	READ_COLOR(battery_color2, buffer[43]);
	text_font = buffer[44];

	if (buffer[0] < 3) return;

	if (i < 47) {
		APP_LOG(APP_LOG_LEVEL_ERROR,
		    "truncated persistent buffer (size %d), using only v2",
		    i);
		return;
	}

	READ_COLOR(minute_hand_color, buffer[45]);
	READ_COLOR(pin_color, buffer[46]);
}

static void
write_config(void) {
	uint8_t buffer[PERSIST_BUFFER_SIZE];
	int i;

	buffer[0] = 3;
	SAVE_COLOR(buffer[1], background_color);
	SAVE_COLOR(buffer[2], battery_color);
	SAVE_COLOR(buffer[3], bluetooth_color);
	SAVE_COLOR(buffer[4], hour_hand_color);
	SAVE_COLOR(buffer[45], minute_hand_color);
	SAVE_COLOR(buffer[46], pin_color);
	SAVE_COLOR(buffer[5], hour_mark_color);
	SAVE_COLOR(buffer[6], inner_rectangle_color);
	SAVE_COLOR(buffer[7], minute_mark_color);
	SAVE_COLOR(buffer[8], text_color);

	buffer[9] = bluetooth_vibration ? 1 : 0;
	buffer[10] = show_battery_icon_below;

	memcpy(buffer + 11, text_format, sizeof text_format);

	SAVE_COLOR(buffer[43], battery_color2);
	buffer[44] = text_font;

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
#define ICON_LAYER_SET_HIDDEN  do { \
	layer_set_hidden(icon_layer, \
	    (bluetooth_connected || !IS_VISIBLE(bluetooth_color)) \
	    && (has_battery \
	     || !(IS_VISIBLE(battery_color) || IS_VISIBLE(battery_color2)))); \
	} while (0)

#ifdef CACHE_BACKGROUND
#define SCREEN_BUFFER_SIZE \
    PBL_IF_RECT_ELSE(PBL_IF_COLOR_ELSE(144 * 168, 144 * 168 / 8), 25868)

static uint8_t background_cache[SCREEN_BUFFER_SIZE];
static bool use_background_cache = false;

static size_t
gbitmap_get_data_size(GBitmap *bitmap) {
	GRect bounds;
	if (!bitmap) return 0;

	bounds = gbitmap_get_bounds(bitmap);
#ifdef PBL_SDK_3
	GBitmapDataRowInfo row_info;
	switch (gbitmap_get_format(bitmap)) {
	    case GBitmapFormat1Bit:
		return bounds.size.w * bounds.size.h / 8;
	    case GBitmapFormat8Bit:
		return bounds.size.w * bounds.size.h;
	    case GBitmapFormat8BitCircular:
		row_info = gbitmap_get_data_row_info(bitmap, bounds.size.h - 1);
		return (row_info.data + row_info.max_x + 1)
		    - gbitmap_get_data(bitmap);
	    default:
		return 0;
	}
#else
	return bounds.size.w * bounds.size.h / 8;
#endif
}

static bool
save_frame_buffer(GContext *ctx) {
	GBitmap *frame_buffer = graphics_capture_frame_buffer(ctx);

	if (!frame_buffer) {
		APP_LOG(APP_LOG_LEVEL_WARNING,
		    "Unable to capture frame buffer for saving");
		return false;
	}

	if (gbitmap_get_data_size(frame_buffer) != SCREEN_BUFFER_SIZE) {
		APP_LOG(APP_LOG_LEVEL_WARNING,
		    "Unexpected frame buffer size %u, expected %u",
		    gbitmap_get_data_size(frame_buffer), SCREEN_BUFFER_SIZE);
		graphics_release_frame_buffer(ctx, frame_buffer);
		return false;
	}

	memcpy(background_cache, gbitmap_get_data(frame_buffer),
	    sizeof background_cache);
	use_background_cache = true;
	graphics_release_frame_buffer(ctx, frame_buffer);
	return true;
}

static bool
restore_frame_buffer(GContext *ctx) {
	GBitmap *frame_buffer = graphics_capture_frame_buffer(ctx);

	if (!frame_buffer) {
		APP_LOG(APP_LOG_LEVEL_WARNING,
		    "Unable to capture frame buffer for restore");
		return false;
	}

	if (gbitmap_get_data_size(frame_buffer) != SCREEN_BUFFER_SIZE) {
		APP_LOG(APP_LOG_LEVEL_WARNING,
		    "Unexpected frame buffer size %u, expected %u",
		    gbitmap_get_data_size(frame_buffer), SCREEN_BUFFER_SIZE);
		graphics_release_frame_buffer(ctx, frame_buffer);
		return false;
	}

	memcpy(gbitmap_get_data(frame_buffer), background_cache,
	    sizeof background_cache);
	graphics_release_frame_buffer(ctx, frame_buffer);
	return true;
}
#endif

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

#ifdef CACHE_BACKGROUND
	if (use_background_cache && restore_frame_buffer(ctx)) return;
#endif

	if (IS_VISIBLE(minute_mark_color)) {
		graphics_context_set_stroke_color(ctx, minute_mark_color);
		INSET_RECT(rect, bounds, 5);
		for (i = 0; i < 60; i += 1) {
			point_at_angle(&rect, TRIG_MAX_ANGLE * i / 60,
			    &pt1, &horiz);
			pt2.x = pt1.x + horiz;
			pt2.y = pt1.y + (1 - horiz);
			graphics_draw_line(ctx, pt1, pt2);
		}
	}

	if (IS_VISIBLE(hour_mark_color)) {
#ifndef PBL_BW
		graphics_context_set_stroke_width(ctx, 3);
#endif

		graphics_context_set_stroke_color(ctx, hour_mark_color);
		INSET_RECT(rect,  bounds, 11);
		INSET_RECT(rect2, bounds, 22);
		for (i = 0; i < 12; i += 1) {
			point_at_angle(&rect, TRIG_MAX_ANGLE * i / 12,
			    &pt1, &horiz);
			point_at_angle(&rect2, TRIG_MAX_ANGLE * i / 12,
			    &pt2, &horiz);

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
	}

	if (IS_VISIBLE(inner_rectangle_color)) {
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
		for (i = rect.origin.y +2;
		    i < rect.origin.y + rect.size.h;
		    i += 3) {
			pt1.y = pt2.y = i;
			graphics_draw_pixel(ctx, pt1);
			graphics_draw_pixel(ctx, pt2);
		}
#else
		graphics_draw_rect(ctx, rect);
#endif
	}

#ifdef CACHE_BACKGROUND
	save_frame_buffer(ctx);
#endif
}
#else
static GPoint
point_at_angle(GRect bounds, int32_t angle) {
	GPoint ret;
	uint32_t width = bounds.size.w;
	uint32_t height = bounds.size.h;
	ret.x = bounds.origin.x
	    + (TRIG_MAX_RATIO * (width + 1) + width * sin_lookup(angle))
	    / (2 * TRIG_MAX_RATIO);
	ret.y = bounds.origin.y
	    + (TRIG_MAX_RATIO * (height + 1) - height * cos_lookup(angle))
	    / (2 * TRIG_MAX_RATIO);
	return ret;
}

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

#ifdef CACHE_BACKGROUND
	if (use_background_cache && restore_frame_buffer(ctx)) return;
#endif

	if (IS_VISIBLE(minute_mark_color)) {
		graphics_context_set_stroke_color(ctx, minute_mark_color);
		INSET_RECT(rect, bounds, 5);
		for (i = 0; i < 60; i += 1) {
			angle = TRIG_MAX_ANGLE * i / 60;
			graphics_draw_line(ctx,
			    point_at_angle(rect, angle - angle_delta),
			    point_at_angle(rect, angle + angle_delta));
		}
	}

	if (IS_VISIBLE(hour_mark_color)) {
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
	}

	if (IS_VISIBLE(inner_rectangle_color)) {
		graphics_context_set_stroke_width(ctx, 1);
		graphics_context_set_stroke_color(ctx, inner_rectangle_color);
		graphics_draw_circle(ctx, center, radius - 35);
	}

#ifdef CACHE_BACKGROUND
	save_frame_buffer(ctx);
#endif
}
#endif

static void
hand_layer_draw(Layer *layer, GContext *ctx) {
	(void)layer;

	graphics_context_set_fill_color(ctx, hour_hand_color);
	graphics_context_set_stroke_color(ctx, background_color);

	gpath_rotate_to(minute_hand_path, TRIG_MAX_ANGLE * tm_now.tm_min / 60);
	gpath_draw_filled(ctx, minute_hand_path);
	gpath_draw_outline(ctx, minute_hand_path);

	graphics_context_set_fill_color(ctx, minute_hand_color);

	gpath_rotate_to(hour_hand_path,
	    TRIG_MAX_ANGLE * (tm_now.tm_hour * 60 + tm_now.tm_min) / 720);
	gpath_draw_filled(ctx, hour_hand_path);
	gpath_draw_outline(ctx, hour_hand_path);

#ifdef CACHE_BACKGROUND
	if (!use_background_cache) return;
#endif

	graphics_context_set_fill_color(ctx, background_color);
	graphics_fill_circle(ctx, center, 2);
	graphics_context_set_fill_color(ctx, pin_color);
	graphics_fill_circle(ctx, center, 1);
}

static void
icon_layer_draw(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds);
	GPoint pt;

	if (!bluetooth_connected && IS_VISIBLE(bluetooth_color)) {
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

	if (!has_battery
	    && (IS_VISIBLE(battery_color) || IS_VISIBLE(battery_color2))) {
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
		if (!IS_EQUAL(battery_color2, battery_color)) {
			graphics_context_set_fill_color(ctx, battery_color2);
			graphics_fill_rect(ctx,
			    GRect(pt.x + 5, pt.y + 1, 4, 5),
			    0, GCornerNone);
			graphics_fill_rect(ctx,
			    GRect(pt.x + 13, pt.y + 1, 4, 5),
			    0, GCornerNone);
		}
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

static void
update_text_font(unsigned new_text_font) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	if (new_text_font >= TEXT_FONT_NUMBER) return;

	if (text_layer) {
		if (new_text_font == text_font) return;
		text_layer_destroy(text_layer);
	}

	text_font = new_text_font;

	text_layer = text_layer_create(GRect(
	    bounds.origin.x,
	    bounds.origin.y + text_offsets[text_font],
	    bounds.size.w,
	    text_heights[text_font]));
	text_layer_set_text_color(text_layer, text_color);
	text_layer_set_background_color(text_layer, GColorClear);
	text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
	text_layer_set_font(text_layer,
	    fonts_get_system_font(text_fonts[text_font]));
	layer_set_hidden(text_layer_get_layer(text_layer),
	    !text_format[0] || !IS_VISIBLE(text_color));
	layer_insert_below_sibling(text_layer_get_layer(text_layer),
	    icon_layer);
}

/********************
 * SERVICE HANDLERS *
 ********************/

static void
battery_handler(BatteryChargeState charge) {
	if (current_battery == charge.charge_percent) return;
	current_battery = charge.charge_percent;
	ICON_LAYER_SET_HIDDEN;
	if (!has_battery) layer_mark_dirty(icon_layer);
}

static void
bluetooth_handler(bool connected) {
	bluetooth_connected = connected;
	ICON_LAYER_SET_HIDDEN;
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
			hour_hand_color = color_from_tuple(tuple);
			pin_color = minute_hand_color = hour_hand_color;
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
		    case 9:
			battery_color2 = color_from_tuple(tuple);
			layer_mark_dirty(icon_layer);
			break;
		    case 10:
			if (tuple->type != TUPLE_INT
			    && tuple->type != TUPLE_UINT)
				APP_LOG(APP_LOG_LEVEL_ERROR,
				    "bad type %d for text_font entry",
				    (int)tuple->type);
			else if (tuple->value->uint8 == 0
			    || tuple->value->uint8 > TEXT_FONT_NUMBER)
				APP_LOG(APP_LOG_LEVEL_ERROR,
				    "bad value %u for text_font entry",
				    (unsigned)tuple->value->uint8);
			else
				update_text_font(tuple->value->uint8 - 1);
				update_text_layer(&tm_now);
			break;
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
			break;
		    case 20:
			hour_hand_color = color_from_tuple(tuple);
			layer_mark_dirty(hand_layer);
			break;
		    case 21:
			minute_hand_color = color_from_tuple(tuple);
			layer_mark_dirty(hand_layer);
			break;
		    case 22:
			pin_color = color_from_tuple(tuple);
			layer_mark_dirty(hand_layer);
			break;
		    default:
			APP_LOG(APP_LOG_LEVEL_ERROR,
			    "unknown configuration key %lu",
			    (unsigned long)tuple->key);
		}
	}

#ifdef CACHE_BACKGROUND
	use_background_cache = !IS_VISIBLE(inner_rectangle_color)
	    && !IS_VISIBLE(hour_mark_color)
	    && !IS_VISIBLE(minute_mark_color);
	layer_set_hidden(background_layer, use_background_cache);
#else
	layer_set_hidden(background_layer,
	    !IS_VISIBLE(inner_rectangle_color)
	    && !IS_VISIBLE(hour_mark_color)
	    && !IS_VISIBLE(minute_mark_color));
#endif

	ICON_LAYER_SET_HIDDEN;

	layer_set_hidden(text_layer_get_layer(text_layer),
	    !text_format[0] || !IS_VISIBLE(text_color));

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
#ifdef CACHE_BACKGROUND
	use_background_cache = !IS_VISIBLE(inner_rectangle_color)
	    && !IS_VISIBLE(hour_mark_color)
	    && !IS_VISIBLE(minute_mark_color);
	layer_set_hidden(background_layer, use_background_cache);
#else
	layer_set_hidden(background_layer,
	    !IS_VISIBLE(inner_rectangle_color)
	    && !IS_VISIBLE(hour_mark_color)
	    && !IS_VISIBLE(minute_mark_color));
#endif
	layer_add_child(window_layer, background_layer);

	icon_layer = layer_create(GRect(bounds.origin.x
	    + (bounds.size.w - 33) / 2, PBL_IF_RECT_ELSE(97, 105), 33, 36));
	layer_set_update_proc(icon_layer, &icon_layer_draw);
	ICON_LAYER_SET_HIDDEN;
	layer_add_child(window_layer, icon_layer);

	update_text_font(text_font);
	update_text_layer(&tm_now);

	hand_layer = layer_create(bounds);
	layer_set_update_proc(hand_layer, &hand_layer_draw);
	layer_add_child(window_layer, hand_layer);
}

static void
window_unload(Window *window) {
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
	hour_hand_color = GColorBlack;
	minute_hand_color = GColorBlack;
	pin_color = GColorBlack;
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
	battery_color2 = battery_color;

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
	app_message_open(1024, 0);
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
