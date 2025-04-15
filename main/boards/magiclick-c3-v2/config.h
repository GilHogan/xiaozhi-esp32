#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_WS GPIO_NUM_8
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_6
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_7
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_10

#define AUDIO_CODEC_PA_PIN GPIO_NUM_11  
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_3
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_4
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR 

#define BUILTIN_LED_NUM 1
#define BUILTIN_LED_GPIO GPIO_NUM_0

#define BOOT_BUTTON_GPIO GPIO_NUM_2

//battery
#define BUILTIN_BATTERY_GPIO GPIO_NUM_1

// display
#define DISPLAY_SDA_PIN GPIO_NUM_13
#define DISPLAY_SCL_PIN GPIO_NUM_12
#define DISPLAY_CS_PIN GPIO_NUM_20
#define DISPLAY_DC_PIN GPIO_NUM_21
#define DISPLAY_RST_PIN GPIO_NUM_NC

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 128
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_9
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#endif // _BOARD_CONFIG_H_
