#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "lvgl/widgets/lv_canvas.h"
#include "lvgl/widgets/lv_chart.h"
#include "lvgl/widgets/lv_dropdown.h"
#include "lvgl/widgets/lv_roller.h"
#include "lvgl/widgets/lv_slider.h"
#include "lvgl/widgets/lv_table.h"
#include "lvgl/widgets/lv_tabview.h"
#include "lvgl/widgets/lv_textarea.h"
#include "lvgl/widgets/lv_tileview.h"
#include "lvgl/widgets/lv_list.h"
#include "lvgl/widgets/lv_menu.h"

/* Legacy SDK applications can still use upstream compatibility widgets.
 * Suppress deprecation only in this deliberate compatibility contract. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
static void legacy_navigation(lv_obj_t *screen)
{
    lv_obj_t *list=lv_list_create(screen);assert(list);
    assert(lv_list_add_text(list,"Settings"));
    lv_obj_t *button=lv_list_add_button(list,LV_SYMBOL_SETTINGS,"Display");assert(button);
    assert(strcmp(lv_list_get_button_text(list,button),"Display")==0);
    lv_obj_delete(button);
    button=lv_list_add_button(list,NULL,"Audio");assert(button);
    assert(strcmp(lv_list_get_button_text(list,button),"Audio")==0);
    lv_obj_delete(list);

    lv_obj_t *menu=lv_menu_create(screen);assert(menu);
    lv_obj_t *home=lv_menu_page_create(menu,"Home");
    lv_obj_t *details=lv_menu_page_create(menu,"Details");assert(home && details);
    lv_obj_t *entry=lv_menu_cont_create(home);assert(entry);
    lv_menu_set_load_page_event(menu,entry,details);
    lv_menu_set_page(menu,home);assert(lv_menu_get_cur_main_page(menu)==home);
    lv_obj_send_event(entry,LV_EVENT_CLICKED,NULL);
    assert(lv_menu_get_cur_main_page(menu)==details);
    lv_menu_set_page(menu,home);assert(lv_menu_get_cur_main_page(menu)==home);
    lv_obj_delete(menu);
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(void)
{
    lv_init();
    lv_display_t * display = lv_display_create(320, 240);
    assert(display != NULL);
    lv_obj_t * screen = lv_screen_active();
    assert(screen != NULL);

    lv_obj_t * canvas = lv_canvas_create(screen);
    assert(canvas != NULL);

    lv_obj_t * chart = lv_chart_create(screen);
    assert(chart != NULL);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 8);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    assert(lv_chart_get_type(chart) == LV_CHART_TYPE_LINE);
    assert(lv_chart_get_point_count(chart) == 8);
    assert(lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y) != NULL);

    lv_obj_t * dropdown = lv_dropdown_create(screen);
    assert(dropdown != NULL);
    lv_dropdown_set_options(dropdown, "One\nTwo\nThree");
    lv_dropdown_set_selected(dropdown, 1);
    assert(lv_dropdown_get_option_count(dropdown) == 3);
    assert(lv_dropdown_get_selected(dropdown) == 1);

    lv_obj_t * roller = lv_roller_create(screen);
    assert(roller != NULL);
    lv_roller_set_options(roller, "Low\nMedium\nHigh", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, 2, LV_ANIM_OFF);
    assert(lv_roller_get_option_count(roller) == 3);
    assert(lv_roller_get_selected(roller) == 2);

    lv_obj_t * slider = lv_slider_create(screen);
    assert(slider != NULL);
    lv_slider_set_range(slider, -10, 90);
    lv_slider_set_value(slider, 25, LV_ANIM_OFF);
    assert(lv_slider_get_min_value(slider) == -10);
    assert(lv_slider_get_max_value(slider) == 90);
    assert(lv_slider_get_value(slider) == 25);

    lv_obj_t * table = lv_table_create(screen);
    assert(table != NULL);
    lv_table_set_row_count(table, 2);
    lv_table_set_column_count(table, 2);
    lv_table_set_cell_value(table, 0, 0, "Name");
    lv_table_set_cell_value(table, 1, 1, "Value");
    assert(strcmp(lv_table_get_cell_value(table, 0, 0), "Name") == 0);
    assert(lv_table_get_row_count(table) == 2);
    assert(lv_table_get_column_count(table) == 2);

    lv_obj_t * tabview = lv_tabview_create(screen);
    assert(tabview != NULL);
    assert(lv_tabview_add_tab(tabview, "Main") != NULL);
    assert(lv_tabview_add_tab(tabview, "Diag") != NULL);
    lv_tabview_set_active(tabview, 1, LV_ANIM_OFF);
    assert(lv_tabview_get_tab_count(tabview) == 2);
    assert(lv_tabview_get_tab_active(tabview) == 1);

    lv_obj_t * textarea = lv_textarea_create(screen);
    assert(textarea != NULL);
    lv_textarea_set_text(textarea, "hello");
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_password_mode(textarea, true);
    assert(strcmp(lv_textarea_get_text(textarea), "hello") == 0);
    assert(lv_textarea_get_one_line(textarea));
    assert(lv_textarea_get_password_mode(textarea));

    lv_obj_t * tileview = lv_tileview_create(screen);
    assert(tileview != NULL);
    lv_obj_t * tile0 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);
    lv_obj_t * tile1 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT);
    assert(tile0 != NULL && tile1 != NULL);
    lv_tileview_set_tile_by_index(tileview, 1, 0, LV_ANIM_OFF);
    assert(lv_tileview_get_tile_active(tileview) == tile1);

    legacy_navigation(screen);
    lv_obj_delete(screen);
    lv_display_delete(display);
    lv_deinit();
    puts("PASS native widget lifecycle and basic state contract");
    return 0;
}
