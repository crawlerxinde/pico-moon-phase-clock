#include "moon_intro.h"

#include <lvgl.h>
#include <ui.h>

namespace
{
    void set_zoom(void *obj, int32_t value)
    {
        lv_img_set_zoom((lv_obj_t *)obj, (uint16_t)value);
    }

    void set_x(void *obj, int32_t value)
    {
        lv_obj_set_x((lv_obj_t *)obj, (lv_coord_t)value);
    }

    void set_opa(void *obj, int32_t value)
    {
        lv_obj_set_style_img_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
    }

    constexpr uint32_t INTRO_DELAY_MS = 100;
    constexpr uint32_t INTRO_TIME_MS  = 620;

    void animate(lv_obj_t *moon, lv_anim_exec_xcb_t exec,
                 int32_t from, int32_t to, uint32_t delay)
    {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, moon);
        lv_anim_set_exec_cb(&anim, exec);
        lv_anim_set_values(&anim, from, to);
        lv_anim_set_time(&anim, INTRO_TIME_MS);
        lv_anim_set_delay(&anim, delay);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_start(&anim);
    }
}

namespace MoonIntro
{
    void play()
    {
        lv_obj_t *moon = ui_Screen1_Image_Moon;
        if (!moon) return;

        lv_anim_del(moon, NULL);

        /* The screen slide takes ~180 ms; let the moon arrive with it.
         * Start small/offset left, then settle into the normal 170 px tile. */
        lv_obj_set_style_img_opa(moon, 0, 0);
        lv_obj_set_x(moon, -36);
        animate(moon, set_opa, 0, 255, INTRO_DELAY_MS);
        animate(moon, set_zoom, 130, 236, INTRO_DELAY_MS);
        animate(moon, set_x, -36, 0, INTRO_DELAY_MS);
    }
}
