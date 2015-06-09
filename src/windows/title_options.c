/*****************************************************************************
 * Copyright (c) 2014 Ted John, Timmy Weerwag
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * This file is part of OpenRCT2.
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "../addresses.h"
#include "../config.h"
#include "../game.h"
#include "../localisation/localisation.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../interface/themes.h"

enum {
	WIDX_OPTIONS,
	WIDX_CONTENT_BROWSER,
};

static rct_widget window_title_options_widgets[] = {
	{ WWT_DROPDOWN_BUTTON,	2,	0,	120,	0,	11, STR_OPTIONS,			STR_NONE },
	{ WWT_DROPDOWN_BUTTON,	2,	0,	120,	11, 22, STR_CONTENT_BROWSER,	STR_NONE },
	{ WIDGETS_END },
};

static void window_title_options_emptysub() {}
static void window_title_options_paint();
static void window_title_options_mouseup();
static void window_title_options_invalidate();

static void* window_title_options_events[] = {
	window_title_options_emptysub,
	window_title_options_mouseup,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_emptysub,
	window_title_options_invalidate,
	window_title_options_paint,
	window_title_options_emptysub
};

/**
 * Creates the window containing the options button on the title screen.
 */
void window_title_options_open()
{
	rct_window* window;

	window = window_create(
		RCT2_GLOBAL(RCT2_ADDRESS_SCREEN_WIDTH, sint16) - 120, 0,
		120, 23,
		(uint32*)window_title_options_events,
		WC_TITLE_OPTIONS,
		WF_STICK_TO_BACK
	);
	window->widgets = window_title_options_widgets;
	window->enabled_widgets = (1 << WIDX_OPTIONS) | (1 << WIDX_CONTENT_BROWSER);
	window_init_scroll_widgets(window);
	window->flags |= 16;
}

static void window_title_options_mouseup()
{
	short widgetIndex;
	rct_window *w;

	window_widget_get_registers(w, widgetIndex);

	if (RCT2_GLOBAL(RCT2_ADDRESS_RUN_INTRO_TICK_PART, uint8) != 0)
		return;

	switch (widgetIndex){
	case WIDX_OPTIONS:
		window_options_open();
		break;
	case WIDX_CONTENT_BROWSER:
		window_content_browser_open();
		break;
	}
}

static void window_title_options_paint()
{
	rct_window *w;
	rct_drawpixelinfo *dpi;

	window_paint_get_registers(w, dpi);

	window_draw_widgets(w, dpi);
}

static void window_title_options_invalidate()
{
	rct_window *w;
	window_get_register(w);
	colour_scheme_update(w);
}
