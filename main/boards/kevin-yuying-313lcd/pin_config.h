
#pragma once
#define GC9503V_LCD_H_RES 376
#define GC9503V_LCD_V_RES 960


#define GC9503V_LCD_LVGL_DIRECT_MODE            (1)
#define GC9503V_LCD_LVGL_AVOID_TEAR             (1)
#define GC9503V_LCD_RGB_BOUNCE_BUFFER_MODE      (1)
#define GC9503V_LCD_DRAW_BUFF_DOUBLE            (0)
#define GC9503V_LCD_DRAW_BUFF_HEIGHT            (100)
#define GC9503V_LCD_RGB_BUFFER_NUMS             (2)
#define GC9503V_LCD_RGB_BOUNCE_BUFFER_HEIGHT    (10)

#define GC9503V_LCD_PIXEL_CLOCK_HZ (16 * 1000 * 1000)
#define GC9503V_LCD_BK_LIGHT_ON_LEVEL 1
#define GC9503V_LCD_BK_LIGHT_OFF_LEVEL !GC9503V_LCD_BK_LIGHT_ON_LEVEL
#define GC9503V_PIN_NUM_BK_LIGHT GPIO_NUM_4
#define GC9503V_PIN_NUM_HSYNC 6
#define GC9503V_PIN_NUM_VSYNC 5
#define GC9503V_PIN_NUM_DE 15
#define GC9503V_PIN_NUM_PCLK 7

#define GC9503V_PIN_NUM_DATA0 47 // B0
#define GC9503V_PIN_NUM_DATA1 21 // B1
#define GC9503V_PIN_NUM_DATA2 14 // B2
#define GC9503V_PIN_NUM_DATA3 13 // B3
#define GC9503V_PIN_NUM_DATA4 12 // B4

#define GC9503V_PIN_NUM_DATA5 11  // G0
#define GC9503V_PIN_NUM_DATA6 10  // G1
#define GC9503V_PIN_NUM_DATA7 9   // G2
#define GC9503V_PIN_NUM_DATA8 46  // G3
#define GC9503V_PIN_NUM_DATA9 3   // G4
#define GC9503V_PIN_NUM_DATA10 20 // G5

#define GC9503V_PIN_NUM_DATA11 19 // R0
#define GC9503V_PIN_NUM_DATA12 8  // R1
#define GC9503V_PIN_NUM_DATA13 18 // R2
#define GC9503V_PIN_NUM_DATA14 17 // R3
#define GC9503V_PIN_NUM_DATA15 16 // R4

#define GC9503V_PIN_NUM_DISP_EN -1

#define GC9503V_LCD_IO_SPI_CS_1 (GPIO_NUM_48)
#define GC9503V_LCD_IO_SPI_SCL_1 (GPIO_NUM_17)
#define GC9503V_LCD_IO_SPI_SDO_1 (GPIO_NUM_16)