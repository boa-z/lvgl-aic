#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "lvgl/widgets/lv_arc.h"
#include "lvgl/widgets/lv_button.h"
#include "lvgl/widgets/lv_buttonmatrix.h"
#include "lvgl/widgets/lv_calendar.h"
#include "lvgl/widgets/lv_checkbox.h"
#include "lvgl/widgets/lv_keyboard.h"
#include "lvgl/widgets/lv_led.h"
#include "lvgl/widgets/lv_line.h"
#include "lvgl/widgets/lv_msgbox.h"
#include "lvgl/widgets/lv_spinbox.h"
#include "lvgl/widgets/lv_switch.h"

int main(void)
{
    lv_init();
    lv_display_t * display = lv_display_create(320, 240);
    assert(display != NULL);
    lv_obj_t * screen = lv_screen_active();
    assert(screen != NULL);

    assert(lv_button_create(screen) != NULL);

    lv_obj_t * arc = lv_arc_create(screen);
    assert(arc != NULL);
    lv_arc_set_range(arc, -20, 80);
    lv_arc_set_value(arc, 25);
    lv_arc_set_angles(arc, 15, 300);
    assert(lv_arc_get_min_value(arc) == -20);
    assert(lv_arc_get_max_value(arc) == 80);
    assert(lv_arc_get_value(arc) == 25);
    assert(lv_arc_get_angle_start(arc) == 15);
    assert(lv_arc_get_angle_end(arc) == 300);

    lv_obj_t * checkbox = lv_checkbox_create(screen);
    assert(checkbox != NULL);
    lv_checkbox_set_text(checkbox, "Ready");
    assert(strcmp(lv_checkbox_get_text(checkbox), "Ready") == 0);

    lv_obj_t * textarea = lv_textarea_create(screen);
    assert(textarea != NULL);
    lv_obj_t * keyboard = lv_keyboard_create(screen);
    assert(keyboard != NULL);
    lv_keyboard_set_textarea(keyboard, textarea);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_popovers(keyboard, true);
    assert(lv_keyboard_get_textarea(keyboard) == textarea);
    assert(lv_keyboard_get_mode(keyboard) == LV_KEYBOARD_MODE_NUMBER);
    assert(lv_keyboard_get_popovers(keyboard));

    lv_obj_t * spinbox = lv_spinbox_create(screen);
    assert(spinbox != NULL);
    lv_spinbox_set_range(spinbox, -100, 100);
    lv_spinbox_set_step(spinbox, 5);
    lv_spinbox_set_value(spinbox, 35);
    lv_spinbox_set_rollover(spinbox, true);
    assert(lv_spinbox_get_min_value(spinbox) == -100);
    assert(lv_spinbox_get_max_value(spinbox) == 100);
    assert(lv_spinbox_get_step(spinbox) == 5);
    assert(lv_spinbox_get_value(spinbox) == 35);
    assert(lv_spinbox_get_rollover(spinbox));

    lv_obj_t * buttonmatrix = lv_buttonmatrix_create(screen);
    assert(buttonmatrix != NULL);
    static const char * map[] = {"A", "B", "", "C", NULL};
    lv_buttonmatrix_set_map(buttonmatrix, map);
    lv_buttonmatrix_set_selected_button(buttonmatrix, 1);
    assert(strcmp(lv_buttonmatrix_get_button_text(buttonmatrix, 1), "B") == 0);
    assert(lv_buttonmatrix_get_selected_button(buttonmatrix) == 1);

    lv_obj_t * calendar = lv_calendar_create(screen);
    assert(calendar != NULL);
    lv_calendar_set_today_date(calendar, 2026, 10, 1);
    lv_calendar_set_month_shown(calendar, 2026, 10);
    static lv_calendar_date_t highlighted[] = {{2026, 10, 5}, {2026, 10, 12}};
    lv_calendar_set_highlighted_dates(calendar, highlighted, 2);
    assert(lv_calendar_get_today_date(calendar)->year == 2026);
    assert(lv_calendar_get_showed_date(calendar)->month == 10);
    assert(lv_calendar_get_highlighted_dates_num(calendar) == 2);

    lv_obj_t * led = lv_led_create(screen);
    assert(led != NULL);
    lv_led_set_brightness(led, 96);
    assert(lv_led_get_brightness(led) == 96);

    lv_obj_t * line = lv_line_create(screen);
    assert(line != NULL);
    static const lv_point_precise_t points[] = {{0, 0}, {10, 20}, {20, 5}};
    lv_line_set_points(line, points, 3);
    lv_line_set_y_invert(line, true);
    assert(lv_line_get_point_count(line) == 3);
    assert(lv_line_get_points(line) == points);
    assert(lv_line_get_y_invert(line));

    lv_obj_t * msgbox = lv_msgbox_create(screen);
    assert(msgbox != NULL);
    assert(lv_msgbox_add_title(msgbox, "Notice") != NULL);
    assert(lv_msgbox_add_text(msgbox, "Ready") != NULL);
    assert(lv_msgbox_add_footer_button(msgbox, "OK") != NULL);
    assert(lv_msgbox_get_title(msgbox) != NULL);
    assert(lv_msgbox_get_content(msgbox) != NULL);
    assert(lv_msgbox_get_footer(msgbox) != NULL);

    lv_obj_t * sw = lv_switch_create(screen);
    assert(sw != NULL);

    lv_obj_delete(screen);
    lv_display_delete(display);
    lv_deinit();
    puts("PASS common control widget lifecycle and state contract");
    return 0;
}
