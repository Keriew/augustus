#ifndef GRAPHICS_UI_RUNTIME_H
#define GRAPHICS_UI_RUNTIME_H

#include "graphics/color.h"
#include "graphics/image_button.h"
#include "graphics/scrollbar.h"

#include "graphics/ui_primitives.h"

class SharedUiRuntime {
public:
    void draw_button_border(
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        color_t color);

    void draw_outer_panel(int x, int y, int width_blocks, int height_blocks);
    void draw_unbordered_panel(int x, int y, int width_blocks, int height_blocks, color_t color);
    void draw_inner_panel(int x, int y, int width_blocks, int height_blocks);
    void draw_label(int x, int y, int width_blocks, int type);
    void draw_large_label(int x, int y, int width_blocks, int type);
    int draw_top_menu_black_panel(int x, int y, int width);

    void draw_image_button(int x, int y, const image_button &button);
    void draw_scrollbar_dot(const scrollbar_type &scrollbar);

    int save_region(int image_id, int x, int y, int width, int height);
    void draw_saved_region(int image_id, int x, int y);

private:
    UiPrimitives primitives_;
};

SharedUiRuntime &shared_ui_runtime(void);

#endif // GRAPHICS_UI_RUNTIME_H
