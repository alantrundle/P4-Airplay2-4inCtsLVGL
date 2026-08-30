#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "gfx_driver.h"


LGFX_ST7796::LGFX_ST7796()
{
    // ============================================================
    // SPI BUS
    // ============================================================
    {
        auto cfg = _bus.config();

        cfg.spi_host = SPI2_HOST;
        cfg.spi_mode = 0;

        // Deliberately slow for initial display testing.
        cfg.freq_write = 40'000'000;
        cfg.freq_read  = 2000000;

        cfg.spi_3wire = false;
        cfg.use_lock  = true;

        // No DMA while debugging.
        cfg.dma_channel = 1;

        cfg.pin_sclk = TFT_SCLK;   // GPIO52
        cfg.pin_mosi = TFT_MOSI;   // GPIO51
        cfg.pin_miso = TFT_MISO;   // GPIO31
        cfg.pin_dc   = TFT_DC;     // GPIO39

        _bus.config(cfg);

        _panel.setBus(&_bus);
    }


    // ============================================================
    // ST7796 PANEL
    // ============================================================
    {
        auto cfg = _panel.config();

        cfg.pin_cs   = TFT_CS;      // GPIO30
        cfg.pin_rst  = TFT_RST;     // GPIO28
        cfg.pin_busy = -1;

        cfg.panel_width   = 320;
        cfg.panel_height  = 480;

        cfg.memory_width  = 320;
        cfg.memory_height = 480;

        cfg.offset_x = 0;
        cfg.offset_y = 0;

        cfg.offset_rotation = 0;

        cfg.readable = false;

        cfg.invert    = false;
        cfg.rgb_order = false;

        cfg.dlen_16bit = false;
        cfg.bus_shared = false;

        _panel.config(cfg);
    }


    // ============================================================
    // TOUCH
    // ============================================================
    {
        auto cfg = _touch.config();

        cfg.x_min = 0;
        cfg.y_min = 0;

        cfg.x_max = 319;
        cfg.y_max = 479;

        cfg.i2c_port = 1;
        cfg.i2c_addr = TOUCH_ADDR;

        cfg.pin_sda = I2C_SDA;
        cfg.pin_scl = I2C_SCL;

        cfg.pin_int = TOUCH_INT;
        cfg.pin_rst = TOUCH_RST;

        cfg.freq = 100000;

        _touch.config(cfg);

        _panel.setTouch(&_touch);
    }


    setPanel(&_panel);
}


bool LGFX_ST7796::readTouch(
    uint16_t &x,
    uint16_t &y,
    bool &pressed
)
{
    int32_t tx = 0;
    int32_t ty = 0;

    if (getTouch(&tx, &ty))
    {
        if (tx < 0) tx = 0;
        if (ty < 0) ty = 0;

        if (tx >= TFT_HOR_RES)
            tx = TFT_HOR_RES - 1;

        if (ty >= TFT_VER_RES)
            ty = TFT_VER_RES - 1;

        x = static_cast<uint16_t>(tx);
        y = static_cast<uint16_t>(ty);

        pressed = true;

        return true;
    }

    pressed = false;

    return false;
}


LGFX_ST7796 display;