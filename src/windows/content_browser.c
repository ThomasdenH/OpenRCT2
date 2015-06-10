#include "../addresses.h"
#include "../audio/audio.h"
#include "../editor.h"
#include "../localisation/localisation.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../sprites.h"
#include "../interface/themes.h"
#include "../network/http.h"

#include "math.h"

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
	char* description;
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

// This is the actual index in the above list, NOT in the on screen list
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

uint32 window_content_browser_enabled_widgets = (1 << WIDX_CLOSE) | (1 << WIDX_FILTER_TEXT) | (1 << WIDX_CONTENT_BROWSER) | (1 << WIDX_TAB_1) | (1 << WIDX_TAB_2) | (1 << WIDX_TAB_3);

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
	window_content_browser_emptysub,
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
	browser_items = malloc(sizeof(content_browser_item*) * TYPE_COUNT);
	for (int x = 0; x < TYPE_COUNT; x++){
		item_count[x] = 0;
	}

	download_list(window);
}

static void handle_json(http_json_response *response){
	if (response == NULL)
		return;

	rct_window *window = window_bring_to_front_by_class(WC_CONTENT_BROWSER);
	if (window == NULL)
		// The window has been closed
		return;

	// Find out the type
	const char* name = json_string_value(json_object_get(response->root, "name"));
	if (name == NULL)
		return;
	int content_type = -1;

	if (strcmp(name, "Tracks") == 0)
		content_type = TYPE_TRACK;
	else if (strcmp(name, "Themes") == 0)
		content_type = TYPE_THEME;
	else if (strcmp(name, "Scenarios") == 0)
		content_type = TYPE_SCENARIO;
	else return;

	// The number of items
	item_count[content_type] = (int)json_integer_value(json_object_get(response->root, "numberOfItems"));
	if (item_count[content_type] <= 0)
		return;

	browser_items[content_type] = malloc(sizeof(content_browser_item) * item_count[content_type]);

	json_t *json_items = json_object_get(response->root, "items");
	if (json_items == NULL)
		return;

	char number[256];
	for (int index = 0; index < item_count[content_type]; index++){

		sprintf(number, "%d", index + 1);
		json_t *item = json_object_get(json_items, number);

		if (item != NULL){

			int string_length;

			// Copy title
			const char* title = json_string_value(json_object_get(item, "title"));
			if (title == NULL)
				title = "INVALID TITLE";
			string_length = strlen(title);
			browser_items[content_type][index].title = malloc(sizeof(char) * string_length + 1);
			browser_items[content_type][index].title[string_length] = '\0';
			memcpy(browser_items[content_type][index].title, title, string_length);

			browser_items[content_type][index].downloads = (int)json_integer_value(json_object_get(item, "downloads"));

			browser_items[content_type][index].featured = json_boolean_value(json_object_get(item, "featured"));

			browser_items[content_type][index].type = content_type;

			// Copy url
			const char* url = json_string_value(json_object_get(item, "downloadUrl"));
			if (url == NULL)
				url = "URL NOT FOUND";
			string_length = strlen(url);
			browser_items[content_type][index].url = malloc(sizeof(char) * string_length + 1);
			browser_items[content_type][index].url[string_length] = '\0';
			memcpy(browser_items[content_type][index].url, url, string_length);
			
			// Description
			const char* description = json_string_value(json_object_get(item, "description"));
			if (description == NULL)
				description = "DESCRIPTION NOT FOUND";
			string_length = strlen(description);
			browser_items[content_type][index].description = malloc(sizeof(char) * string_length + 1);
			browser_items[content_type][index].description[string_length] = '\0';
			memcpy(browser_items[content_type][index].description, description, string_length);

		}
		else
			index--;
	}
	
	window_content_browser_list_update(window);
}

static void download_list(rct_window *w){
	http_request_json_async(URL_TRACKS, &handle_json);
	http_request_json_async(URL_THEMES, &handle_json);
	http_request_json_async(URL_SCENARIOS, &handle_json);
}

static bool already_installed(content_browser_item item){
	if (item.type == TYPE_THEME){
		char path[MAX_PATH];
		platform_get_user_directory(path, "themes");
		strcat(path, item.title);
		strcat(path, ".ini");
		return platform_file_exists(path);
	}
	return false;
}

static bool allow_install(content_browser_item item){
	return !already_installed(item) && item.type != TYPE_TRACK && item.type != TYPE_SCENARIO;
}

static void install_item(content_browser_item *item){
	if (item->type == TYPE_THEME){
		char path[MAX_PATH];
		platform_get_user_directory(path, "themes");
		strcat(path, item->title);
		strcat(path, ".zip");
		if (!http_download_file(item->url, path))
			puts("Error");
	}
}

static void free_memory(){
	for (int type = 0; type < TYPE_COUNT; type++){
		for (int index = 0; index < item_count[type]; index++){
			free(browser_items[type][index].title);
			free(browser_items[type][index].url);
			free(browser_items[type][index].description);
		}
		free(browser_items[type]);
	}
	free(browser_items);
	free(item_count);
}

static void window_content_browser_close(){
	free_memory();
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

	clicked_item = w->list_item_positions[index];

	// Disable or enable the download button
	if (allow_install(browser_items[w->page][clicked_item]))
		w->enabled_widgets |= 1 << WIDX_DOWNLOAD_BUTTON;
	else
		w->enabled_widgets &= ~(1 << WIDX_DOWNLOAD_BUTTON);
	window_init_scroll_widgets(w);
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

	gfx_draw_string_left_clipped(dpi, STR_CONTENT_NAME,			NULL, 1,								w->x + 240,		w->y + WH - 127,	100);
	gfx_draw_string_left_clipped(dpi, STR_CONTENT_DESCRIPTION,	NULL, 1,								w->x + 240,		w->y + WH - 116,	100);
	gfx_draw_string_left_clipped(dpi, STR_CONTENT_DOWNLOADS,	NULL, 1,								w->x + 240,		w->y + WH - 50,		100);

	if (clicked_item != -1){
		gfx_draw_string_left_clipped(dpi, 1170, &(browser_items[w->page][clicked_item].title),	1,		w->x + 340,		w->y + WH - 127,	WW - 340 - 10);

		char buffer[1024];
		int font_height;
		int num_lines;
		memcpy(buffer, browser_items[w->page][clicked_item].description, 1024);
		gfx_wrap_string(buffer, WW - 340 - 10, &num_lines, &font_height);
		for (int x = 0; x < min(num_lines, 6); x++){
			char* line = buffer;
			int found = 0;
			while (found < x){
				if (*line == 0)
					found++;
				line++;
			}
			gfx_draw_string(dpi, line, 1, w->x + 340, w->y + WH - 116 + x * 11);
		}

		gfx_draw_string_left_clipped(dpi, 5182, &(browser_items[w->page][clicked_item].downloads), 1,	w->x + 340,		w->y + WH - 50,		WW - 340 - 10);
	}
}

static void window_content_browser_scrollpaint(){
	rct_window *w;
	rct_drawpixelinfo *dpi;

	window_paint_get_registers(w, dpi);

	int y;

	for (int i = 0; i < w->no_list_items; i++) {
		
		int list_position = w->list_item_positions[i];

		y = i * 10;

		if (i == w->selected_list_item) {
			gfx_fill_rect(dpi, 0, y, 800, y + 9, 0x02000031);
			gfx_draw_string(dpi, browser_items[w->page][list_position].title, 1, 5, y);
		} else {
			gfx_draw_string(dpi, browser_items[w->page][list_position].title, 5, 5, y);
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

int currentPage;

int compar(const void* p1, const void* p2){
	return strcmp(browser_items[currentPage][*((uint8*)p1)].title, browser_items[currentPage][*((uint8*)p2)].title);
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

	currentPage = w->page;
	qsort(w->list_item_positions, index, sizeof(uint8), &compar);

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