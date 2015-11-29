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
static GColor bluetooth_color;
static GColor hand_color;
static GColor hour_mark_color;
static GColor inner_rectangle_color;
static GColor minute_mark_color;
static GColor text_color;
static const char *text_font = FONT_KEY_GOTHIC_14;
static const char text_format[] = "%a %d";
static bool bluetooth_vibration = true;

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

static const GPathInfo minute_hand_path_points
	= QUAD_PATH_POINTS(15, 6, -72, -6);
static const GPathInfo hour_hand_path_points
	= QUAD_PATH_POINTS(15, 7, -50, -7);
static const GPathInfo bluetooth_logo_points = { 7, (GPoint[]) {
	{ -4, -4 },
	{  4,  4 },
	{  0,  8 },
	{  0, -8 },
	{  4, -4 },
	{ -4,  4 },
	{  0,  0 } } };
static const GPathInfo bluetooth_frame_points = { 3, (GPoint[]) {
	{ -15,  11 },
	{   0, -15 },
	{  15,  11 } } };

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


static void background_layer_draw(Layer *layer, GContext *ctx) {
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
	gpath_move_to(bluetooth_frame, center);
	gpath_move_to(bluetooth_logo, center);
	graphics_context_set_stroke_color(ctx, bluetooth_color);
	gpath_draw_outline(ctx, bluetooth_frame);
#ifdef PBL_SDK_3
	gpath_draw_outline_open(ctx, bluetooth_logo);
#else
	gpath_draw_outline(ctx, bluetooth_logo);
#endif
}

static void
update_text_layer(struct tm *time) {
	strftime(text_buffer, sizeof text_buffer, text_format, time);
	text_layer_set_text(text_layer, text_buffer);
}

/********************
 * SERVICE HANDLERS *
 ********************/

static void
bluetooth_handler(bool connected) {
	bluetooth_connected = connected;
	layer_set_hidden(icon_layer, connected);
	layer_mark_dirty(icon_layer);

	if (bluetooth_vibration && !connected) vibes_long_pulse();
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
	    + (bounds.size.w - 31) / 2, PBL_IF_RECT_ELSE(100, 110), 31, 31));
	layer_set_update_proc(icon_layer, &icon_layer_draw);
	layer_set_hidden(icon_layer, bluetooth_connected);
	layer_add_child(window_layer, icon_layer);

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

	bluetooth_connected = connection_service_peek_pebble_app_connection();

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	background_color = GColorWhite;
	bluetooth_color = GColorBlack;
	hand_color = GColorBlack;
	hour_mark_color = GColorBlack;
	minute_mark_color = GColorBlack;
	text_color = GColorBlack;

#ifdef PBL_BW
	inner_rectangle_color = GColorBlack;
#else
	inner_rectangle_color = GColorLightGray;
#endif

	bluetooth_frame = gpath_create(&bluetooth_frame_points);
	bluetooth_logo = gpath_create(&bluetooth_logo_points);
	hour_hand_path = gpath_create(&hour_hand_path_points);
	minute_hand_path = gpath_create(&minute_hand_path_points);

	connection_service_subscribe(((ConnectionHandlers){
	    .pebble_app_connection_handler = &bluetooth_handler,
	    .pebblekit_connection_handler = 0}));
	tick_timer_service_subscribe(MINUTE_UNIT, &tick_handler);
	window_stack_push(window, true);
}

static void
deinit(void) {
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
