// Compact month calendar for the right-hand column.
//
// The calendar is a real 7x6 label grid (not a proportional text block), so
// the columns stay aligned with Inter while the today cell can be outlined.

#include "ui.h"

#include <stdio.h>

#define CAL_X       270
#define CAL_Y       8
#define CAL_W       180
#define CAL_H       120
#define CAL_CELL_W  24
#define CAL_GRID_X  8
#define CAL_DAY_Y   56

static lv_obj_t *cal_root;
static lv_obj_t *cal_today;
static lv_obj_t *cal_month;
static lv_obj_t *cal_year;
static lv_obj_t *cal_days[42];

static const char *cal_weekdays[7] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};

static int cal_days_in_month(int year, int month)
{
    static const int mdays[12] = {31, 28, 31, 30, 31, 30,
                                  31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    int days = mdays[month - 1];
    int leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
    if (month == 2 && leap) days = 29;
    return days;
}

// 0 = Monday ... 6 = Sunday, for the first day of the month.
static int cal_first_monday(int year, int month)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = year;
    if (month < 3) y -= 1;
    int w = (y + y / 4 - y / 100 + y / 400 + t[month - 1] + 1) % 7;
    if (w < 0) w += 7;
    return (w + 6) % 7;
}

static lv_obj_t *cal_line(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, h);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_opa(line, 255, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

void Calendar_Init(lv_obj_t *parent)
{
    cal_root = lv_obj_create(parent);
    lv_obj_remove_style_all(cal_root);
    lv_obj_set_pos(cal_root, CAL_X, CAL_Y);
    lv_obj_set_size(cal_root, CAL_W, CAL_H);
    lv_obj_clear_flag(cal_root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_obj_create(cal_root);
    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, CAL_W, 32);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    cal_month = lv_label_create(header);
    lv_label_set_text(cal_month, "Oct");
    lv_obj_set_style_text_font(cal_month, &ui_font_Inter28, 0);
    lv_obj_set_style_text_color(cal_month, lv_color_hex(0xF2F2F2), 0);

    cal_year = lv_label_create(header);
    lv_label_set_text(cal_year, "2026");
    lv_obj_set_style_text_font(cal_year, &ui_font_Inter28, 0);
    lv_obj_set_style_text_color(cal_year, lv_color_hex(0xF2F2F2), 0);

    cal_line(cal_root, 0, 32, CAL_W, 1);

    for (int c = 0; c < 7; c++) {
        lv_obj_t *wd = lv_label_create(cal_root);
        lv_obj_set_pos(wd, c * CAL_CELL_W + CAL_GRID_X, 38);
        lv_obj_set_size(wd, CAL_CELL_W, 12);
        lv_label_set_text(wd, cal_weekdays[c]);
        lv_obj_set_style_text_align(wd, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(wd, &ui_font_Inter10M, 0);
        lv_obj_set_style_text_color(wd, lv_color_hex(0xBEBEBE), 0);
    }

    cal_today = lv_obj_create(cal_root);
    lv_obj_remove_style_all(cal_today);
    lv_obj_set_pos(cal_today, CAL_GRID_X + 1, CAL_DAY_Y);
    lv_obj_set_size(cal_today, 22, 10);
    lv_obj_set_style_bg_opa(cal_today, 0, 0);
    lv_obj_set_style_border_color(cal_today, lv_color_hex(0xEDEDED), 0);
    lv_obj_set_style_border_width(cal_today, 1, 0);
    lv_obj_set_style_border_opa(cal_today, 255, 0);
    lv_obj_set_style_radius(cal_today, 5, 0);
    lv_obj_add_flag(cal_today, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cal_today, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 42; i++) {
        lv_obj_t *day = lv_label_create(cal_root);
        lv_obj_set_pos(day, (i % 7) * CAL_CELL_W + CAL_GRID_X,
                       CAL_DAY_Y + (i / 7) * 10);
        lv_obj_set_size(day, CAL_CELL_W, 10);
        lv_label_set_text(day, "");
        lv_obj_set_style_text_align(day, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(day, &ui_font_Inter11R, 0);
        lv_obj_set_style_text_color(day, lv_color_hex(0xF0F0F0), 0);
        cal_days[i] = day;
    }

    cal_line(cal_root, 0, 118, CAL_W, 1);
}

void Calendar_Update(int year, int month, int day)
{
    if (!cal_root) return;

    char buf[8];
    static const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (month < 1 || month > 12) return;

    lv_label_set_text(cal_month, month_names[month - 1]);
    snprintf(buf, sizeof(buf), "%d", year);
    lv_label_set_text(cal_year, buf);

    const int days = cal_days_in_month(year, month);
    const int first = cal_first_monday(year, month);
    const int weeks = (first + days + 6) / 7;
    const int step = (weeks == 6) ? 10 : 12;

    for (int i = 0; i < 42; i++) {
        lv_obj_set_pos(cal_days[i], (i % 7) * CAL_CELL_W + CAL_GRID_X,
                       CAL_DAY_Y + (i / 7) * step);
        lv_obj_set_size(cal_days[i], CAL_CELL_W, step);
        int n = i - first + 1;
        if (n >= 1 && n <= days) {
            snprintf(buf, sizeof(buf), "%d", n);
            lv_label_set_text(cal_days[i], buf);
        } else {
            lv_label_set_text(cal_days[i], "");
        }
    }

    const int idx = first + day - 1;
    const int row = idx / 7;
    const int col = idx % 7;
    lv_obj_set_pos(cal_today, col * CAL_CELL_W + CAL_GRID_X + 1,
                   CAL_DAY_Y + row * step);
    lv_obj_set_size(cal_today, 22, step);
    if (day < 1 || day > days) {
        lv_obj_add_flag(cal_today, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(cal_today, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_background(cal_today);
}
