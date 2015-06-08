#include "../addresses.h"
#include "../audio/audio.h"
#include "../editor.h"
#include "../localisation/localisation.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../sprites.h"
#include "../interface/themes.h"

#define WW 600
#define WH 400

typedef struct {
	char* title;
	bool featured;
	int downloads;
} content_browser_item;

enum {
	PAGE_TRACKS,
	PAGE_SCENARIOS,
	PAGE_THEMES,
	PAGE_COUNT,
};

content_browser_item* browser_items;
int* item_count;
int clicked_item = -1;

enum {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE,
	WIDX_CONTENT_BROWSER,
	WIDX_TAB_1,
	WIDX_TAB_2,
	WIDX_TAB_3,
};

content_browser_item testitems[] = {
	{"Item 1", false, 10},
	{ "Item 2", false, 10 },
	{ "Item 3", false, 10 },
	{ "Item 4", false, 10 },
};

int testItemCount[] = {
	4, 0, 0
};

uint32 window_content_browser_enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_CONTENT_BROWSER) | (1 << WIDX_TAB_1) | (1 << WIDX_TAB_2) | (1 << WIDX_TAB_3);

static rct_widget window_content_browser_widgets[] = {
	{ WWT_FRAME,			0,	0,		599,	0,		399,	0xFFFFFFFF,				STR_NONE								},
	{ WWT_CAPTION,			0,	1,		598,	1,		14,		STR_CONTENT_BROWSER,	STR_WINDOW_TITLE_TIP					},
	{ WWT_CLOSEBOX,			0,	587,	597,	2,		13,		STR_CLOSE_X,			STR_CLOSE_WINDOW_TIP					},
	{ WWT_SCROLL,			0,	4,		221,	45,		395,	STR_NONE,				STR_NONE								},
	{ WWT_TAB,              0,	3,		33,		17,		43,     0x2000144E,				STR_NONE								},
	{ WWT_TAB,              0,	34,		64,		17,		43,     0x2000144E,				STR_NONE								},
	{ WWT_TAB,              0,	65,		95,		17,		43,     0x2000144E,				STR_NONE								},
	{ WIDGETS_END },
};

static void window_content_browser_emptysub() { }
static void window_content_browser_close();
static void window_content_browser_mouseup();
static void window_content_browser_scrollgetsize();
static void window_content_browser_scrollmousedown();
static void window_content_browser_scrollmouseover();
static void window_content_browser_tooltip();
static void window_content_browser_invalidate();
static void window_content_browser_paint();
static void window_content_browser_scrollpaint();
static void window_content_browser_list_populate(content_browser_item* items, int count[], rct_window *w);
static void window_content_browser_list_update(rct_window *w);

static void* window_content_browser_events[] = {
	window_content_browser_close,
	window_content_browser_mouseup,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_scrollgetsize,
	window_content_browser_scrollmousedown,
	window_content_browser_emptysub,
	window_content_browser_scrollmouseover,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_tooltip,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_invalidate,
	window_content_browser_paint,
	window_content_browser_scrollpaint
};

void window_content_browser_open(){
	rct_window *window;

	// Check if window is already open
	window = window_bring_to_front_by_class(WC_CONTENT_BROWSER);
	if (window != NULL)
		return;

	window = window_create_centred(WW, WH, (uint32*)window_content_browser_events, WC_CONTENT_BROWSER, 0);
	window->widgets = window_content_browser_widgets;
	window->enabled_widgets = window_content_browser_enabled_widgets;
	window->page = PAGE_TRACKS;
	window_init_scroll_widgets(window);
	window_content_browser_list_populate(testitems, testItemCount, window);
}

static void window_content_browser_close(){
	rct_window *w;
	window_get_register(w)
	window_close(w);
}

static void window_content_browser_mouseup(){
	short widgetIndex;
	rct_window *w;

	window_widget_get_registers(w, widgetIndex);

	switch (widgetIndex) {
	case WIDX_TAB_1:
	case WIDX_TAB_2:
	case WIDX_TAB_3:
		w->page = widgetIndex - WIDX_TAB_1;
		window_content_browser_list_update(w);
		break;
	case WIDX_CLOSE:
		window_close(w);
		break;
	}
}

static void window_content_browser_scrollgetsize()
{
	int top, width, height;
	rct_window *w;

	window_get_register(w);

	height = w->no_list_items * 10;
	if (w->selected_list_item != -1) {
		w->selected_list_item = -1;
		window_invalidate(w);
	}

	top = height - window_content_browser_widgets[WIDX_CONTENT_BROWSER].bottom + window_content_browser_widgets[WIDX_CONTENT_BROWSER].top + 21;
	if (top < 0)
		top = 0;
	if (top < w->scrolls[0].v_top) {
		w->scrolls[0].v_top = top;
		window_invalidate(w);
	}

	width = 0;
	window_scrollsize_set_registers(width, height);
}

static void window_content_browser_scrollmousedown(){
	int index;
	short x, y, scrollIndex;
	rct_window *w;

	window_scrollmouse_get_registers(w, scrollIndex, x, y);

	index = y / 10;
	if (index >= w->no_list_items)
		return;

	clicked_item = index;
}

static void window_content_browser_scrollmouseover(){
	int index;
	short x, y, scrollIndex;
	rct_window *w;

	window_scrollmouse_get_registers(w, scrollIndex, x, y);

	index = y / 10;
	if (index >= w->no_list_items)
		return;

	w->selected_list_item = index;
}

static void window_content_browser_tooltip(){

}

static void window_content_browser_invalidate(){
	rct_window *w;

	window_get_register(w);
	colour_scheme_update(w);
}

static void window_content_browser_paint(){
	rct_window *w;
	rct_drawpixelinfo *dpi;

	window_paint_get_registers(w, dpi);

	window_draw_widgets(w, dpi);

	if (clicked_item != -1){
		char buffer[265];
		sprintf(buffer, "Name: %s", browser_items[clicked_item].title);
		gfx_draw_string(dpi, buffer, 1, 400, 400);
		sprintf(buffer, "Downloads: %d", browser_items[clicked_item].downloads);
		gfx_draw_string(dpi, buffer, 1, 400, 411);
	}
}

static void window_content_browser_scrollpaint(){
	rct_window *w;
	rct_drawpixelinfo *dpi;

	window_paint_get_registers(w, dpi);

	int y;

	for (int i = 0; i < w->no_list_items; i++) {
		
		y = i * 10;

		content_browser_item* item = browser_items + w->list_item_positions[i];

		if (i == w->selected_list_item) {
			gfx_fill_rect(dpi, 0, y, 800, y + 9, 0x02000031);
			gfx_draw_string(dpi, item->title, 1, 5, y);
		} else {
			gfx_draw_string(dpi, item->title, 5, 5, y);
		}
		
	}
}

static int filter(content_browser_item item){
	return 1;
}

static void window_content_browser_list_populate(content_browser_item* items, int count[], rct_window *w){

	item_count = count;
	browser_items = items;
	window_content_browser_list_update(w);
}

static void window_content_browser_list_update(rct_window *w){

	int begin = w->page == 0 ? 0 : item_count[w->page - 1];
	int end = item_count[w->page];

	w->selected_list_item = -1;
	clicked_item = -1;
	w->no_list_items = item_count[w->page];
	int index = 0;
	for (int x = begin; x < end; x++){
		if (filter(browser_items[x]))
			index++;
			w->list_item_positions[index] = x;
	}

	window_invalidate(w);
}