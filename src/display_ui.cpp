#include "display_ui.h"
#include "gfx_driver.h"
#include "ui/ui_manager.h"

#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

static bool display_ok = false;
static lv_display_t *lvgl_display = nullptr;
static lv_indev_t *touch_indev = nullptr;
static lv_color_t *disp_draw_buf = nullptr;
static uint32_t last_tick = 0;

static void set_backlight_percent(int percent)
{
    percent = constrain(percent, 0, 100);
    display.setBrightness(static_cast<uint8_t>(percent * 255 / 100));
}

static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    display.startWrite();
    display.setAddrWindow(area->x1, area->y1, w, h);
    display.writePixels(reinterpret_cast<uint16_t *>(px_map),
                        static_cast<uint32_t>(w * h), true);
    display.endWrite();

    lv_display_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    uint16_t x = 0;
    uint16_t y = 0;
    bool pressed = false;
    display.readTouch(x, y, pressed);

    if (!pressed) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = constrain(x, 0, TFT_HOR_RES - 1);
    data->point.y = constrain(y, 0, TFT_VER_RES - 1);
}

bool display_ui_begin()
{
    Serial.println("Starting LovyanGFX ST7796 + LVGL display");
    Serial.printf("LCD SPI: MOSI=%d MISO=%d SCLK=%d CS=%d DC=%d RST=%d BL=%d\n",
                  TFT_MOSI, TFT_MISO, TFT_SCLK, TFT_CS, TFT_DC, TFT_RST, TFT_BL);
    Serial.printf("Touch: SDA=%d SCL=%d RST=%d INT=%d addr=0x%02X\n",
                  I2C_SDA, I2C_SCL, TOUCH_RST, TOUCH_INT, TOUCH_ADDR);

    if (!display.init()) {
        Serial.println("LovyanGFX/ST7796 init failed");
        return false;
    }

    display.setRotation(PANEL_ROTATION);
    set_backlight_percent(100);
    display.fillScreen(TFT_BLACK);

    if (display.width() != TFT_HOR_RES || display.height() != TFT_VER_RES) {
        Serial.printf("WARNING: display geometry is %dx%d, expected %dx%d\n",
                      display.width(), display.height(), TFT_HOR_RES, TFT_VER_RES);
    }

    lv_init();

    // Larger partial buffer reduces the number of SPI transactions per frame.
    // This does not change the known-good 4 MHz / DMA-off LCD driver settings.
    const size_t buffer_pixels = static_cast<size_t>(TFT_HOR_RES) * 8;
    disp_draw_buf = static_cast<lv_color_t *>(heap_caps_malloc(
        sizeof(lv_color_t) * buffer_pixels,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!disp_draw_buf) {
        disp_draw_buf = static_cast<lv_color_t *>(heap_caps_malloc(
            sizeof(lv_color_t) * buffer_pixels,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    if (!disp_draw_buf) {
        Serial.println("LVGL draw buffer allocation failed");
        return false;
    }

    lvgl_display = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    if (!lvgl_display) {
        Serial.println("LVGL display creation failed");
        return false;
    }

    lv_display_set_color_format(lvgl_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lvgl_display, my_disp_flush);
    lv_display_set_buffers(lvgl_display,
                           disp_draw_buf,
                           nullptr,
                           sizeof(lv_color_t) * buffer_pixels,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, my_touchpad_read);
    lv_indev_set_display(touch_indev, lvgl_display);

    if (!ui_manager_begin(lv_screen_active())) {
        Serial.println("UI manager init warning: RTSP listener registration failed");
    }

    last_tick = millis();
    for (int i = 0; i < 20; ++i) {
        const uint32_t now = millis();
        lv_tick_inc(now - last_tick);
        last_tick = now;
        ui_manager_process(now);
        lv_timer_handler();
        delay(5);
    }

    display_ok = true;
    Serial.println("LovyanGFX + LVGL now-playing UI ready");
    return true;
}

void display_ui_loop()
{
    if (!display_ok) return;

    const uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    ui_manager_process(now);
    lv_timer_handler();
}

void display_ui_set_wifi_connecting()
{
    ui_manager_set_wifi("WiFi: connecting...");
}

void display_ui_set_wifi_connected(const IPAddress &ip)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "WiFi: %s", ip.toString().c_str());
    ui_manager_set_wifi(buf);
}

void display_ui_set_wifi_failed()
{
    ui_manager_set_wifi("WiFi: failed");
}

void display_ui_set_airplay_starting()
{
    ui_manager_set_status("STARTING");
}

void display_ui_set_airplay_started()
{
    ui_manager_set_status("READY");
}

void display_ui_set_airplay_failed()
{
    ui_manager_set_status("FAILED");
}
