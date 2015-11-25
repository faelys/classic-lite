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
static GColor hand_color;

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

/**********************
 * DISPLAY PRIMITIVES *
 **********************/

static const GPathInfo minute_hand_path_points
	= QUAD_PATH_POINTS(15, 6, -72, -6);
static const GPathInfo hour_hand_path_points
	= QUAD_PATH_POINTS(15, 7, -50, -7);

static struct tm tm_now;
static Window *window;
static Layer *hand_layer;
static GPath *hour_hand_path;
static GPath *minute_hand_path;
static GPoint center;

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

/********************
 * SERVICE HANDLERS *
 ********************/

static void
tick_handler(struct tm* tick_time, TimeUnits units_changed) {
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

	hand_layer = layer_create(bounds);
	layer_set_update_proc(hand_layer, &hand_layer_draw);
	layer_add_child(window_layer, hand_layer);
}

static void
window_unload(Window *window) {
	layer_destroy(hand_layer);
}

static void
init(void) {
	time_t current_time = time(0);
	tm_now = *localtime(&current_time);

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	background_color = GColorWhite;
	hand_color = GColorBlack;

	hour_hand_path = gpath_create(&hour_hand_path_points);
	minute_hand_path = gpath_create(&minute_hand_path_points);

	tick_timer_service_subscribe(MINUTE_UNIT, &tick_handler);
	window_stack_push(window, true);
}

static void
deinit(void) {
	tick_timer_service_unsubscribe();
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
