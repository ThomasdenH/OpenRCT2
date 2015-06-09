#include "../addresses.h"
#include "../audio/audio.h"
#include "../editor.h"
#include "../localisation/localisation.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../sprites.h"
#include "../interface/themes.h"
#include "../network/http.h"

#define WW 600
#define WH 400

#define URL_TRACKS "https://openrct.net/api/v2/content/cat/tracks/info.json"
#define URL_THEMES "https://openrct.net/api/v2/content/cat/scenarios/info.json"
#define URL_SCENARIOS "https://openrct.net/api/v2/content/cat/themes/info.json"


typedef struct {
	char* title;
	bool featured;
	int downloads;
	int type;
	char* url;
} content_browser_item;

enum {
	TYPE_TRACK,
	TYPE_SCENARIO,
	TYPE_THEME,
	TYPE_COUNT,
};

enum {
	PAGE_TRACKS,
	PAGE_SCENARIOS,
	PAGE_THEMES,
	PAGE_COUNT,
};

content_browser_item** browser_items;
int* item_count;
int clicked_item = -1;

char _filter_string[41];

enum {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE,
	WIDX_FILTER_LABEL,
	WIDX_FILTER_TEXT,
	WIDX_CONTENT_BROWSER,
	WIDX_TAB_1,
	WIDX_TAB_2,
	WIDX_TAB_3,
	WIDX_DOWNLOAD_BUTTON,
};

uint32 window_content_browser_enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_FILTER_TEXT) | (1 << WIDX_CONTENT_BROWSER) | (1 << WIDX_TAB_1) | (1 << WIDX_TAB_2) | (1 << WIDX_TAB_3) | (1 << WIDX_DOWNLOAD_BUTTON);

static rct_widget window_content_browser_widgets[] = {
	{ WWT_FRAME,			0,	0,			WW - 1,		0,			WH - 1,		0xFFFFFFFF,				STR_NONE								},
	{ WWT_CAPTION,			0,	1,			598,		1,			14,			STR_CONTENT_BROWSER,	STR_WINDOW_TITLE_TIP					},
	{ WWT_CLOSEBOX,			0,	587,		597,		2,			13,			STR_CLOSE_X,			STR_CLOSE_WINDOW_TIP					},
	{ WWT_12,				0,	4,			100,		47,			58,			STR_CONTENT_FILTER,		STR_NONE								},
	{ WWT_TEXT_BOX,			0,	104,		220,		47,			58,			(uint32)_filter_string,	STR_NONE								},
	{ WWT_SCROLL,			0,	4,			221,		65,			WH - 10,	STR_NONE,				STR_NONE								},
	{ WWT_TAB,              0,	3,			33,			17,			43,			0x2000144E,				STR_CONTENT_TRACKS						},
	{ WWT_TAB,              0,	34,			64,			17,			43,			0x2000144E,				STR_CONTENT_SCENARIOS					},
	{ WWT_TAB,              0,	65,			95,			17,			43,			0x2000144E,				STR_CONTENT_THEMES						},
	{ WWT_CLOSEBOX,			0,	WW - 100,	WW - 20,	WH - 24,	WH - 10,	STR_CONTENT_DOWNLOAD,	STR_NONE								},
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
static void window_content_browser_update(rct_window *w);
static void window_content_browser_textinput();

static void window_content_browser_list_populate(content_browser_item** items, int count[], rct_window *w);
static void window_content_browser_list_update(rct_window *w);

static void download_list(rct_window* w);
static void install_item(content_browser_item *item);

static void* window_content_browser_events[] = {
	window_content_browser_close,
	window_content_browser_mouseup,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_emptysub,
	window_content_browser_update,
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
	window_content_browser_textinput,
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

	memset(_filter_string, 0, sizeof(_filter_string));

	window = window_create_centred(WW, WH, (uint32*)window_content_browser_events, WC_CONTENT_BROWSER, 0);
	window->widgets = window_content_browser_widgets;
	window->enabled_widgets = window_content_browser_enabled_widgets;
	window->page = 0;
	window_init_scroll_widgets(window);

	item_count = malloc(sizeof(int) * TYPE_COUNT);
	browser_items = malloc(sizeof(content_browser_item**) * TYPE_COUNT);
	for (int x = 0; x < TYPE_COUNT; x++){
		item_count[x] = 0;
	}

	download_list(window);
}

static void handle_json(http_json_response *response){
	if (response == NULL)
		return;

	const char* name = json_string_value(json_object_get(response->root, "name"));

	int content_type = -1;

	if (strcmp(name, "Tracks")){
		content_type = TYPE_TRACK;
	}
	else if (strcmp(name, "Themes")){
		content_type = TYPE_THEME;
	}
	else if (strcmp(name, "Scenarios")){
		content_type = TYPE_SCENARIO;
	}
	else return;

	item_count[content_type] = (int)json_integer_value(json_object_get(response->root, "numberOfItems"));

	printf("Number of items: %d\n", item_count[content_type]);

	browser_items[content_type] = malloc(sizeof(content_browser_item*) * item_count[content_type]);

	json_t *json_items = json_object_get(response->root, "items");

	char number[256];

	printf("Items: %d\n", json_items);

	for (int index = 0; index < item_count[content_type]; index++){
		sprintf(number, "%d", index + 1);
		json_t *item = json_object_get(json_items, number);
		printf("Name: %s Adress: %d\n", number, item);

		content_browser_item* browser_item = (content_browser_item*)malloc(sizeof(content_browser_item));

		// Copy title
		const char* title = json_string_value(json_object_get(item, "title"));

		if (title == NULL){
			puts("TITLE NULL!");
		}

		browser_item->title = malloc(sizeof(char) * strlen(title) + 1);
		browser_item->title[strlen(title)] = '\0';
		memcpy(browser_item->title, title, strlen(title));

		browser_item->downloads = (int)json_integer_value(json_object_get(item, "downloads"));
		browser_item->featured = json_boolean_value(json_object_get(item, "featured"));
		browser_item->type = content_type;

		// Copy url
		const char* url = json_string_value(json_object_get(item, "downloadUrl"));
		if (url == NULL){
			puts("URL IS NULL!");
		}
		browser_item->url = malloc(sizeof(char) * strlen(url) + 1);
		browser_item->url[strlen(url)] = '\0';
		memcpy(browser_item->url, url, strlen(url));

		printf("%d %d %d %d %d\n", &browser_item->title, &browser_item->downloads, &browser_item->featured, &browser_item->type, &browser_item->url);

		browser_items[content_type][index] = *browser_item;

	}

	rct_window *window = window_bring_to_front_by_class(WC_CONTENT_BROWSER);
	if (window != NULL)
		window_content_browser_list_populate(browser_items, item_count, window);
}

static void download_list(rct_window *w){
	http_request_json_async(URL_TRACKS, &handle_json);
	http_request_json_async(URL_THEMES, &handle_json);
	http_request_json_async(URL_SCENARIOS, &handle_json);
}

static void install_item(content_browser_item *item){

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
	case WIDX_FILTER_TEXT:
		window_start_textbox(w, widgetIndex, 1170, (uint32)_filter_string, 40);
		break;
	case WIDX_DOWNLOAD_BUTTON:
		install_item(browser_items[w->page] + clicked_item);
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

	// Set correct active tab
	for (int i = 0; i < PAGE_COUNT; i++)
		w->pressed_widgets &= ~(1 << (WIDX_TAB_1 + i));
	w->pressed_widgets |= 1LL << (WIDX_TAB_1 + w->page);
}

static void window_content_browser_paint(){
	rct_window *w;
	rct_drawpixelinfo *dpi;

	window_paint_get_registers(w, dpi);

	window_draw_widgets(w, dpi);

	gfx_draw_string_left_clipped(dpi, STR_CONTENT_NAME, NULL, 1, w->x + 240, w->y + WH - 61, 100);
	gfx_draw_string_left_clipped(dpi, STR_CONTENT_DOWNLOADS, NULL, 1, w->x + 240, w->y + WH - 50, 100);

	if (clicked_item != -1){
		
		gfx_draw_string(dpi, browser_items[w->page][clicked_item].title, 1, w->x + 340, w->y + WH - 61);
		char buffer[20];
		sprintf(buffer, "%d", browser_items[w->page][clicked_item].downloads);
		gfx_draw_string(dpi, buffer, 1, w->x + 340, w->y + WH - 50);
	}
}

static void window_content_browser_scrollpaint(){
	rct_window *w;
	rct_drawpixelinfo *dpi;

	window_paint_get_registers(w, dpi);

	int y;

	for (int i = 0; i < w->no_list_items; i++) {
		
		y = i * 10;

		content_browser_item* item = browser_items[w->page] + w->list_item_positions[i];

		if (i == w->selected_list_item) {
			gfx_fill_rect(dpi, 0, y, 800, y + 9, 0x02000031);
			gfx_draw_string(dpi, item->title, 1, 5, y);
		} else {
			gfx_draw_string(dpi, item->title, 5, 5, y);
		}
		
	}
}

static bool filter(content_browser_item item){
	if (_filter_string[0] == 0)
		return true;

	if (item.title[0] == 0)
		return false;

	char name_lower[MAX_PATH];
	char filter_lower[sizeof(_filter_string)];
	strcpy(name_lower, item.title);
	strcpy(filter_lower, _filter_string);

	for (int i = 0; i < (int)strlen(name_lower); i++)
		name_lower[i] = (char)tolower(name_lower[i]);
	for (int i = 0; i < (int)strlen(filter_lower); i++)
		filter_lower[i] = (char)tolower(filter_lower[i]);

	return strstr(name_lower, filter_lower) != NULL;
}

static void window_content_browser_list_populate(content_browser_item** items, int count[], rct_window *w){

	item_count = count;
	browser_items = items;
	window_content_browser_list_update(w);
}

static void window_content_browser_list_update(rct_window *w){

	w->selected_list_item = -1;
	clicked_item = -1;

	int index = 0;

	for (int x = 0; x < item_count[w->page]; x++){
		if (filter(browser_items[w->page][x])){
			w->list_item_positions[index] = x;
			index++;
		}
	}

	w->no_list_items = index;

	window_invalidate(w);
}

static void window_content_browser_update(rct_window *w)
{
	if (gCurrentTextBox.window.classification == w->classification &&
		gCurrentTextBox.window.number == w->number) {
		window_update_textbox_caret();
		widget_invalidate(w, WIDX_FILTER_TEXT);
	}
}

static void window_content_browser_textinput(){
	uint8 result;
	short widgetIndex;
	rct_window *w;
	char *text;

	window_textinput_get_registers(w, widgetIndex, result, text);

	if (widgetIndex != WIDX_FILTER_TEXT || !result)
		return;

	if (strcmp(_filter_string, text) == 0)
		return;

	if (strlen(text) == 0) {
		memset(_filter_string, 0, sizeof(_filter_string));
	}
	else {
		memset(_filter_string, 0, sizeof(_filter_string));
		strcpy(_filter_string, text);
	}

	window_content_browser_list_update(w);
}