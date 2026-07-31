#ifndef GRAPHICS_TAB_VIEW_H
#define GRAPHICS_TAB_VIEW_H

#include "graphics/complex_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "input/mouse.h"

typedef struct content_area content_area;
typedef struct tab tab;
typedef struct tab_view tab_view;

typedef void (*content_draw_callback)(tab_view *, tab *);
typedef int (*content_mouse_handler)(const mouse *, void *);

typedef enum {
    TAB_VIEW_STYLE_DEFAULT,       // Basic style: single rectangle with red border and texture fill
    TAB_VIEW_STYLE_DEFAULT_SMALL, // like default but small font and less padding
    TAB_VIEW_STYLE_GRAY,          // main-menu-like style
    TAB_VIEW_STYLE_WOOD,          // wood-like style
    TAB_VIEW_STYLE_COLORFUL       // colorful style with gradient background
} tab_view_style;

typedef enum {
    TAB_SPREAD_NONE = 0, // tabs tightly together
    TAB_SPREAD_SMALL = 2,// tabs separated by small margin - 5% of the tab view width
    TAB_SPREAD_WIDE = 8, // tabs spread across the width of the tab view area
    TAB_SPREAD_MAX = -1,  // tabs spread across as much as possible - prioritised over the tab_position parameter
    // but NOT prioritised over TAB_WIDTH_MAX -> in that case, TAB_SPREAD is set to WIDE.
} tab_spread;

typedef enum {
    TAB_WIDTH_EQUAL, // all tabs same width, matching longest tab text, or maximum possible width if not enough space
    TAB_WIDTH_TO_CONTENT, // each tab width matches its content text width + padding
    TAB_WIDTH_MAX, // maximum possible width for each tab, dividing available space equally after accounting for gaps
    TAB_WIDTH_CUSTOM // use *user_data to store custom widths for each tab. 
} tab_width_mode;

typedef enum {
    TAB_POS_LEFT,
    TAB_POS_RIGHT,
    TAB_POS_CENTER,
} tab_position; // indexing starts at 0 on the leftmost tab, regardless of the tab_position

struct content_area {
    int x;
    int y;
    int width;
    int height;
    int auto_indent; // default style-coherent indentation for content zone. If 0, no border/indent is applied
    content_draw_callback draw_callback;
};

struct tab {
    complex_button button;
    int visible;
    int enabled; //to do: disabled but visible - greyed out and unclickable
    content_draw_callback draw_callback;
    int initialised; // flag to indicate whether this tab has been setup with text and draw callback
    void *user_data; // optional extensibility
};

/* Sequence positioning from complex button */

struct tab_view {
    int x;
    int y;
    int width;
    int height;
    int tab_height;  // height of tab buttons

    struct {
        int active_tab;
    } state;

    struct {
        tab_view_style style;
        tab_position position;
        tab_spread spread;
        tab_width_mode width_mode;
        int count;
        font_t tab_font; // default font for tab titles
    } view_properties;

    content_area content;
    tab *tabs;

};

typedef enum
{
    TAB_LAYOUT_OK = 1,

    TAB_ERR_NULL_VIEW = -1,
    TAB_ERR_UNINITIALISED_TAB = -2,
    TAB_ERR_INSUFFICIENT_WIDTH = -3,
    TAB_ERR_CUSTOM_WIDTHS = -4

} tab_layout_result;

/* Public API */
void tab_view_init_simple(tab_view *view, int x, int y, int width, int height, int tab_count, tab_view_style style);
void tab_view_destroy(tab_view *view);

/* Layout and rendering */
int tab_view_layout(tab_view *view); //returns TAB_LAYOUT_OK for successful layout
void tab_view_draw(tab_view *view);
int tab_view_handle_mouse(const mouse *m, tab_view *view);
int tab_view_handle_content_mouse(const tab_view *view, const mouse *m, int source_width, int source_height,
    content_mouse_handler handler, void *user_data);

/* Tab configuration */
void tab_view_init_tab(tab_view *view, int tab_index, content_draw_callback callback, const lang_fragment *frag);
/*when using the below functions, you have to initialise the tab yourself afterwards. Otherwise the layout()
  will see them as uninitialised and throw an error. If you don't want to do that, use the init_tab()*/
void tab_view_set_tab_text(tab_view *view, int tab_index, const lang_fragment *frag);
void tab_view_set_tab_draw_callback(tab_view *view, int tab_index, content_draw_callback callback);

/* Accessors */
int tab_view_get_active_tab(const tab_view *view);
void tab_view_set_active_tab(tab_view *view, int tab_index);

#endif // GRAPHICS_TAB_VIEW_H