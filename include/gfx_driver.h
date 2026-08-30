#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

#ifndef TFT_SCLK
#define TFT_SCLK 52
#endif

#ifndef TFT_MOSI
#define TFT_MOSI 51
#endif

#ifndef TFT_MISO
#define TFT_MISO 31
#endif

#ifndef TFT_CS
#define TFT_CS 30
#endif

#ifndef TFT_DC
#define TFT_DC 39
#endif

#ifndef TFT_RST
#define TFT_RST 28
#endif

#ifndef TFT_BL
#define TFT_BL -1
#endif

#ifndef I2C_SDA
#define I2C_SDA 21
#endif

#ifndef I2C_SCL
#define I2C_SCL 22
#endif

#ifndef TOUCH_RST
#define TOUCH_RST 4
#endif

#ifndef TOUCH_INT
#define TOUCH_INT 5
#endif

#ifndef TOUCH_ADDR
#define TOUCH_ADDR 0x38
#endif

#ifndef TFT_HOR_RES
#define TFT_HOR_RES 480
#endif

#ifndef TFT_VER_RES
#define TFT_VER_RES 320
#endif

#ifndef PANEL_ROTATION
#define PANEL_ROTATION 3
#endif

class LGFX_ST7796 : public lgfx::LGFX_Device
{
public:
    LGFX_ST7796();

    bool readTouch(
        uint16_t &x,
        uint16_t &y,
        bool &pressed
    );

private:
    lgfx::Panel_ST7796 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Touch_FT5x06 _touch;
};

extern LGFX_ST7796 display;