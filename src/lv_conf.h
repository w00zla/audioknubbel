/* clang-format off */
#if 1 /* Set it to "1" to enable the content — MUSS 1 sein, sonst ignoriert LVGL die Config */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0   /* auf 1 setzen wenn Farben falsch/invertiert wirken */
#define LV_COLOR_SCREEN_TRANSP  0
#define LV_COLOR_MIX_ROUND_OFS 0
#define LV_COLOR_CHROMA_KEY     lv_color_hex(0x00ff00)

/*=========================
   MEMORY SETTINGS
 *=========================*/
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE (48U * 1024U)   /* 48 KB interner LVGL-Heap */
    #define LV_MEM_ADR  0
    #if LV_MEM_ADR == 0
        #undef LV_MEM_POOL_INCLUDE
        #undef LV_MEM_POOL_ALLOC
        #undef LV_MEM_POOL_FREE
    #endif
#else
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif

/*====================
   HAL SETTINGS
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD  30   /* ms */
#define LV_INDEV_DEF_READ_PERIOD 30   /* ms */

/* millis() als LVGL-Tick — kein manuelles lv_tick_inc() nötig */
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DPI_DEF 130

/*====================
 * DRAWING
 *====================*/
#define LV_DRAW_COMPLEX      1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_GRADIENT_MAX_STOPS 2
#define LV_GRAD_CACHE_DEF_SIZE 0
#define LV_DITHER_GRADIENT    0
#define LV_DISP_ROT_MAX_BUF   (10*1024)

/*====================
 * GPU — alle aus
 *====================*/
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_SWM341_DMA   0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0

/*====================
 * LOGGING + ASSERTS
 *====================*/
#define LV_USE_LOG                  0
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0
#define LV_ASSERT_HANDLER_INCLUDE   <stdint.h>
#define LV_ASSERT_HANDLER           while(1);

/*==================
 *   FONTS
 *==================*/
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  0
#define LV_FONT_MONTSERRAT_12  0
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  0
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  0
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  0
#define LV_FONT_MONTSERRAT_34  1
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  1
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  1
#define LV_FONT_DEFAULT        &lv_font_montserrat_14
#define LV_USE_FONT_SUBPX       1
#define LV_FONT_SUBPX_BGR       0
#define LV_USE_FONT_COMPRESSED  0
#define LV_USE_FONT_PLACEHOLDER 1

/*==================
 *  TEXT
 *==================*/
#define LV_TXT_ENC                          LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS                  " "
#define LV_TXT_LINE_BREAK_LONG_LEN          0
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN  3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_COLOR_CMD                    "#"
#define LV_USE_BIDI                         0
#define LV_USE_ARABIC_PERSIAN_CHARS         0

/*==================
 * WIDGETS — nur Arc + Label
 *==================*/
#define LV_USE_ARC       1
#define LV_USE_BAR       0
#define LV_USE_BTN       0
#define LV_USE_BTNMATRIX 0
#define LV_USE_CANVAS    0
#define LV_USE_CHECKBOX  0
#define LV_USE_DROPDOWN  0
#define LV_USE_IMG       1
#define LV_USE_LABEL     1
#define LV_LABEL_TEXT_SELECTION  0
#define LV_LABEL_LONG_TXT_HINT   0
#define LV_USE_LINE      1
#define LV_USE_ROLLER    0
#define LV_USE_SLIDER    0
#define LV_USE_SWITCH    0
#define LV_USE_TEXTAREA  0
#define LV_USE_TABLE     0

/*==================
 * EXTRA WIDGETS — alle aus
 * WICHTIG: LV_USE_ANIMIMG MUSS 0 sein (zieht sonst LV_USE_IMG als Dep -> Compile-Fehler)
 *==================*/
#define LV_USE_ANIMIMG   0
#define LV_USE_CALENDAR  0
#define LV_USE_CHART     0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN    0
#define LV_USE_KEYBOARD  0
#define LV_USE_LED       0
#define LV_USE_LIST      0
#define LV_USE_MENU      0
#define LV_USE_METER     0
#define LV_USE_MSGBOX    0
#define LV_USE_SPINBOX   0
#define LV_USE_SPINNER   0
#define LV_USE_TABVIEW   0
#define LV_USE_TILEVIEW  0
#define LV_USE_WIN       0
#define LV_USE_SPAN      0

/*==================
 * LAYOUTS
 *==================*/
#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/*==================
 * THEMES
 *==================*/
#define LV_USE_THEME_DEFAULT             1
#define LV_THEME_DEFAULT_DARK            1   /* dunkles Theme für schwarzen Hintergrund */
#define LV_THEME_DEFAULT_GROW            0
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_BASIC               0
#define LV_USE_THEME_MONO                0

#endif /* LV_CONF_H */
#endif /* End of "Content enable" */
