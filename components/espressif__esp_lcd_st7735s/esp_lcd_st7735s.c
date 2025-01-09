/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp_lcd_st7735s.h"

static const char *TAG = "st7735s";

static esp_err_t panel_st7735s_del(esp_lcd_panel_t *panel);
static esp_err_t panel_st7735s_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_st7735s_init(esp_lcd_panel_t *panel);
static esp_err_t panel_st7735s_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_st7735s_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_st7735s_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_st7735s_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_st7735s_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_st7735s_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct
{
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save current value of LCD_CMD_COLMOD register
    const st7735s_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} st7735s_panel_t;

esp_err_t esp_lcd_new_panel_st7735s(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    st7735s_panel_t *st7735s = NULL;
    gpio_config_t io_conf = {0};

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    st7735s = (st7735s_panel_t *)calloc(1, sizeof(st7735s_panel_t));
    ESP_GOTO_ON_FALSE(st7735s, ESP_ERR_NO_MEM, err, TAG, "no mem for st7735s panel");

    if (panel_dev_config->reset_gpio_num >= 0)
    {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    switch (panel_dev_config->color_space)
    {
    case ESP_LCD_COLOR_SPACE_RGB:
        st7735s->madctl_val = 0;
        break;
    case ESP_LCD_COLOR_SPACE_BGR:
        st7735s->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }
#else
    switch (panel_dev_config->rgb_endian)
    {
    case LCD_RGB_ENDIAN_RGB:
        st7735s->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        st7735s->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb endian");
        break;
    }
#endif

    switch (panel_dev_config->bits_per_pixel)
    {
    case 16: // RGB565
        st7735s->colmod_val = 0x55;
        st7735s->fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        st7735s->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        st7735s->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    st7735s->io = io;
    st7735s->reset_gpio_num = panel_dev_config->reset_gpio_num;
    st7735s->reset_level = panel_dev_config->flags.reset_active_high;
    if (panel_dev_config->vendor_config)
    {
        st7735s->init_cmds = ((st7735s_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds;
        st7735s->init_cmds_size = ((st7735s_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds_size;
    }
    st7735s->base.del = panel_st7735s_del;
    st7735s->base.reset = panel_st7735s_reset;
    st7735s->base.init = panel_st7735s_init;
    st7735s->base.draw_bitmap = panel_st7735s_draw_bitmap;
    st7735s->base.invert_color = panel_st7735s_invert_color;
    st7735s->base.set_gap = panel_st7735s_set_gap;
    st7735s->base.mirror = panel_st7735s_mirror;
    st7735s->base.swap_xy = panel_st7735s_swap_xy;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    st7735s->base.disp_off = panel_st7735s_disp_on_off;
#else
    st7735s->base.disp_on_off = panel_st7735s_disp_on_off;
#endif
    *ret_panel = &(st7735s->base);
    ESP_LOGD(TAG, "new st7735s panel @%p", st7735s);

    // ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d", ESP_LCD_ILI9341_VER_MAJOR, ESP_LCD_ILI9341_VER_MINOR,
    //          ESP_LCD_ILI9341_VER_PATCH);

    return ESP_OK;

err:
    if (st7735s)
    {
        if (panel_dev_config->reset_gpio_num >= 0)
        {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(st7735s);
    }
    return ret;
}

static esp_err_t panel_st7735s_del(esp_lcd_panel_t *panel)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);

    if (st7735s->reset_gpio_num >= 0)
    {
        gpio_reset_pin(st7735s->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del st7735s panel @%p", st7735s);
    free(st7735s);
    return ESP_OK;
}

static esp_err_t panel_st7735s_reset(esp_lcd_panel_t *panel)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);
    esp_lcd_panel_io_handle_t io = st7735s->io;

    // perform hardware reset
    if (st7735s->reset_gpio_num >= 0)
    {
        gpio_set_level(st7735s->reset_gpio_num, st7735s->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(st7735s->reset_gpio_num, !st7735s->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    else
    { // perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5ms before sending new command
    }

    return ESP_OK;
}

typedef struct
{
    uint8_t cmd;
    uint8_t data[16];
    uint8_t data_bytes; // Length of data in above data array; 0xFF = end of cmds.
} lcd_init_cmd_t;

static const lcd_init_cmd_t vendor_specific_init_default[] = {
    {ST7735_SWRESET, {0}, 0x80},                                                                                            // Software reset, 0 args, w/delay 150
    {ST7735_SLPOUT, {0}, 0x80},                                                                                             // Out of sleep mode, 0 args, w/delay 500
    {ST7735_FRMCTR1, {0x05, 0x3A, 0x3A}, 3},                                                                                // Frame rate ctrl - normal mode, 3 args: Rate = fosc/(1x2+40) * (LINE+2C+2D)
    {ST7735_FRMCTR2, {0x05, 0x3A, 0x3A}, 3},                                                                                // Frame rate control - idle mode, 3 args:Rate = fosc/(1x2+40) * (LINE+2C+2D)
    {ST7735_FRMCTR3, {0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A}, 6},                                                              // Frame rate ctrl - partial mode, 6 args:Dot inversion mode. Line inversion mode
    {ST7735_INVCTR, {0x03}, 1},                                                                                             // Display inversion ctrl, 1 arg, no delay:No inversion
    {ST7735_PWCTR1, {0x62, 0x02, 0x04}, 3},                                                                                 // Power control, 3 args, no delay:-4.6V AUTO mode
    {ST7735_PWCTR2, {0xC0}, 1},                                                                                             // Power control, 1 arg, no delay:VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
    {ST7735_PWCTR3, {0x0D, 0x00}, 2},                                                                                       // Power control, 2 args, no delay: Opamp current small, Boost frequency
    {ST7735_PWCTR4, {0x8D, 0x6A}, 2},                                                                                       // Power control, 2 args, no delay: BCLK/2, Opamp current small & Medium low
    {ST7735_PWCTR5, {0x8D, 0xEE}, 2},                                                                                       // Power control, 2 args, no delay:
    {ST7735_VMCTR1, {0x0E}, 1},                                                                                             // Power control, 1 arg, no delay:
    {ST7735_INVON, {0}, 0},                                                                                                 // set inverted mode
                                                                                                                            //{ST7735_INVOFF, {0}, 0},                    // set non-inverted mode
    {ST7735_COLMOD, {0x05}, 1},                                                                                             // set color mode, 1 arg, no delay: 16-bit color
    {ST7735_GMCTRP1, {0x10, 0x0E, 0x02, 0x03, 0x0E, 0x07, 0x02, 0x07, 0x0A, 0x12, 0x27, 0x37, 0x00, 0x0D, 0x0E, 0x10}, 16}, // 16 args, no delay:
    {ST7735_GMCTRN1, {0x10, 0x0E, 0x03, 0x03, 0x0F, 0x06, 0x02, 0x08, 0x0A, 0x13, 0x26, 0x36, 0x00, 0x0D, 0x0E, 0x10}, 16}, // 16 args, no delay:
    {ST7735_NORON, {0}, TFT_INIT_DELAY},                                                                                    // Normal display on, no args, w/delay 10 ms delay
    {ST7735_DISPON, {0}, TFT_INIT_DELAY},                                                                                   // Main screen turn on, no args w/delay 100 ms delay
    {0, {0}, 0xff}};

static esp_err_t panel_st7735s_init(esp_lcd_panel_t *panel)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);
    esp_lcd_panel_io_handle_t io = st7735s->io;

    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "send command failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]){
                                                                          st7735s->madctl_val,
                                                                      },
                                                  1),
                        TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]){
                                                                          st7735s->colmod_val,
                                                                      },
                                                  1),
                        TAG, "send command failed");

    const lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    if (st7735s->init_cmds)
    {
        init_cmds = st7735s->init_cmds;
        init_cmds_size = st7735s->init_cmds_size;
    }
    else
    {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(lcd_init_cmd_t);
    }

    bool is_cmd_overwritten = false;
    for (int i = 0; i < init_cmds_size; i++)
    {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd)
        {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            st7735s->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            st7735s->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten)
        {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        }
        esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes);
        // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        // vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_st7735s_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = st7735s->io;

    x_start += st7735s->x_gap;
    x_end += st7735s->x_gap;
    y_start += st7735s->y_gap;
    y_end += st7735s->y_gap;

    // 0.96 tft屏还需要更改刷屏的偏移量，在函数panel_st7735_draw_bitmap中设置
    esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]){
                                                     (x_start >> 8) & 0xFF,
                                                     (x_start + COLSTART) & 0xFF,
                                                     ((x_end - 1) >> 8) & 0xFF,
                                                     (x_end - 1 + COLSTART) & 0xFF,
                                                 },
                              4);
    esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]){
                                                     (y_start >> 8) & 0xFF,
                                                     (y_start + ROWSTART) & 0xFF,
                                                     ((y_end - 1) >> 8) & 0xFF,
                                                     (y_end - 1 + ROWSTART) & 0xFF,
                                                 },
                              4);

    // define an area of frame memory where MCU can access
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]){
    //                                                                      (x_start >> 8) & 0xFF,
    //                                                                      x_start & 0xFF,
    //                                                                      ((x_end - 1) >> 8) & 0xFF,
    //                                                                      (x_end - 1) & 0xFF,
    //                                                                  },
    //                                               4),
    //                     TAG, "send command failed");
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]){
    //                                                                      (y_start >> 8) & 0xFF,
    //                                                                      y_start & 0xFF,
    //                                                                      ((y_end - 1) >> 8) & 0xFF,
    //                                                                      (y_end - 1) & 0xFF,
    //                                                                  },
    //                                               4),
    //                     TAG, "send command failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * st7735s->fb_bits_per_pixel / 8;
    esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len);

    return ESP_OK;
}

static esp_err_t panel_st7735s_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);
    esp_lcd_panel_io_handle_t io = st7735s->io;
    int command = 0;
    if (invert_color_data)
    {
        command = LCD_CMD_INVON;
    }
    else
    {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_st7735s_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);
    esp_lcd_panel_io_handle_t io = st7735s->io;
    if (mirror_x)
    {
        st7735s->madctl_val |= LCD_CMD_MX_BIT;
    }
    else
    {
        st7735s->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y)
    {
        st7735s->madctl_val |= LCD_CMD_MY_BIT;
    }
    else
    {
        st7735s->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]){st7735s->madctl_val}, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_st7735s_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);
    esp_lcd_panel_io_handle_t io = st7735s->io;
    if (swap_axes)
    {
        st7735s->madctl_val |= LCD_CMD_MV_BIT;
    }
    else
    {
        st7735s->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]){st7735s->madctl_val}, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_st7735s_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);
    st7735s->x_gap = x_gap;
    st7735s->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_st7735s_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    st7735s_panel_t *st7735s = __containerof(panel, st7735s_panel_t, base);
    esp_lcd_panel_io_handle_t io = st7735s->io;
    int command = 0;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    on_off = !on_off;
#endif

    if (on_off)
    {
        command = LCD_CMD_DISPON;
    }
    else
    {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
