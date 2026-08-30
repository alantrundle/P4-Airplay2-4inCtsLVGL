static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

  lv_disp_flush_ready(disp);
}

## Hardware Connection Information

### ESP32-8048S070C Pin Connections

The ESP32-8048S070C is an all-in-one display module with the ESP32-S3 microcontroller and a 7-inch 800x480 RGB LCD panel integrated together. The following table shows the internal connections between the ESP32-S3 and the display:

| Signal | ESP32-S3 Pin | Description |
|--------|--------------|-------------|
| DE     | GPIO 41      | Data Enable signal |
| VSYNC  | GPIO 40      | Vertical Sync signal |
| HSYNC  | GPIO 39      | Horizontal Sync signal |
| PCLK   | GPIO 42      | Pixel Clock signal |
| R0     | GPIO 14      | Red data bit 0 (LSB) |
| R1     | GPIO 21      | Red data bit 1 |
| R2     | GPIO 47      | Red data bit 2 |
| R3     | GPIO 48      | Red data bit 3 |
| R4     | GPIO 45      | Red data bit 4 (MSB) |
| G0     | GPIO 9       | Green data bit 0 (LSB) |
| G1     | GPIO 46      | Green data bit 1 |
| G2     | GPIO 3       | Green data bit 2 |
| G3     | GPIO 8       | Green data bit 3 |
| G4     | GPIO 16      | Green data bit 4 |
| G5     | GPIO 1       | Green data bit 5 (MSB) |
| B0     | GPIO 15      | Blue data bit 0 (LSB) |
| B1     | GPIO 7       | Blue data bit 1 |
| B2     | GPIO 6       | Blue data bit 2 |
| B3     | GPIO 5       | Blue data bit 3 |
| B4     | GPIO 4       | Blue data bit 4 (MSB) |
| BL     | GPIO 2       | Backlight control |

### Physical Setup

1. **Power Supply**:
   - The ESP32-8048S070C can be powered via USB-C connector
   - For standalone operation, connect a 5V power supply to the power pins

2. **Programming Connection**:
   - Connect the USB-C port to your computer for programming
   - The board should appear as a COM port in your system

3. **Additional Peripherals**:
   - The module has unused GPIO pins available for connecting sensors, buttons, or other peripherals
   - I2C, SPI, and UART interfaces are available on the expansion headers

### Wiring Diagram

```
+---------------------+
|                     |
|  +-------------+    |
|  |             |    |
|  |   ESP32-S3  |    |
|  |             |    |
|  +-------------+    |
|        |            |
|        |            |
|        v            |
|  +-------------+    |
|  |             |    |
|  |  RGB Display |    |
|  |  800x480    |    |
|  |             |    |
|  +-------------+    |
|                     |
+---------------------+
      ESP32-8048S070C
```

### Important Notes

1. **Do Not Change Pin Assignments**:
   - The pin definitions in the code must match the hardware connections
   - Changing pin assignments in the code will not change the physical connections

2. **Handling Precautions**:
   - The display is sensitive to static electricity
   - Handle the board by the edges and avoid touching the display surface
   - Keep the board in an anti-static bag when not in use

3. **Operating Conditions**:
   - Operating temperature: 0°C to 50°C
   - Avoid exposure to moisture and direct sunlight

## Performance Optimization Tips

### Memory Management

1. **Buffer Optimization**:
   - Use the smallest possible buffer size that meets your needs
   - For full-screen updates, consider using partial updates instead
   - Example: `gfx->updateScreen()` is more efficient than redrawing everything

2. **PSRAM Usage**:
   - If your ESP32-S3 has PSRAM, enable it in the platformio.ini:
     ```ini
     board_build.arduino.memory_type = qio_opi
     ```
   - Store large assets (images, fonts) in PSRAM
   - Example: `lv_img_dsc_t* img = (lv_img_dsc_t*)ps_malloc(size);`

3. **Static Allocation**:
   - Prefer static allocation over dynamic when possible
   - Avoid frequent `new`/`delete` or `malloc`/`free` operations in rendering loops
   - Pre-allocate buffers at initialization time

### Rendering Performance

1. **Minimize Drawing Operations**:
   - Batch similar drawing operations together
   - Avoid redrawing static elements
   - Use clipping regions for partial updates

2. **Color Depth Considerations**:
   - Lower color depths require less memory and processing
   - Consider if your application needs full 16-bit color or if 8-bit would suffice

3. **Font Optimization**:
   - Use bitmap fonts for small text
   - Limit the number of different fonts and sizes
   - Pre-render text that doesn't change frequently

### Power Efficiency

1. **Backlight Control**:
   - Implement backlight dimming or timeout for power saving
   - Example:
     ```cpp
     // Dim backlight after inactivity
     if (millis() - lastActivityTime > 30000) {
       // Dim to 50%
       analogWrite(TFT_BL, 128);
     }
     ```

2. **Refresh Rate Management**:
   - Reduce refresh rate when displaying static content
   - Use event-driven updates rather than continuous polling

3. **Sleep Modes**:
   - Implement deep sleep when the display is not in use
   - Wake on touch or button press

## Example Projects

### 1. Basic Weather Station

This example displays current weather data with a simple UI:

```cpp
#include <Arduino.h>  
#include <Arduino_GFX_Library.h>

// Display setup code as shown earlier...

// Weather data structure
struct WeatherData {
  float temperature;
  float humidity;
  float pressure;
  String condition;
};

WeatherData currentWeather = {23.5, 65.0, 1013.2, "Partly Cloudy"};

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  gfx->begin();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  gfx->fillScreen(BLACK);
  
  // Draw static UI elements
  drawWeatherUI();
  
  // Update with current data
  updateWeatherDisplay(currentWeather);
}

void loop() {
  // In a real application, you would update weather data periodically
  delay(10000);
}

void drawWeatherUI() {
  // Draw header
  gfx->fillRect(0, 0, screenWidth, 60, NAVY);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, 20);
  gfx->print("Weather Station");
  
  // Draw sections for data
  gfx->drawRect(20, 80, 200, 100, WHITE);  // Temperature section
  gfx->drawRect(240, 80, 200, 100, WHITE); // Humidity section
  gfx->drawRect(460, 80, 200, 100, WHITE); // Pressure section
  
  // Labels
  gfx->setTextSize(2);
  gfx->setCursor(30, 90);
  gfx->print("Temperature");
  gfx->setCursor(250, 90);
  gfx->print("Humidity");
  gfx->setCursor(470, 90);
  gfx->print("Pressure");
}

void updateWeatherDisplay(WeatherData data) {
  // Clear previous values
  gfx->fillRect(30, 120, 180, 50, BLACK);
  gfx->fillRect(250, 120, 180, 50, BLACK);
  gfx->fillRect(470, 120, 180, 50, BLACK);
  
  // Display new values
  gfx->setTextColor(YELLOW);
  gfx->setTextSize(3);
  
  gfx->setCursor(30, 130);
  gfx->print(data.temperature, 1);
  gfx->print(" C");
  
  gfx->setCursor(250, 130);
  gfx->print(data.humidity, 0);
  gfx->print(" %");
  
  gfx->setCursor(470, 130);
  gfx->print(data.pressure, 0);
  gfx->print(" hPa");
  
  // Weather condition
  gfx->fillRect(0, 200, screenWidth, 50, BLACK);
  gfx->setTextColor(WHITE);
  gfx->setCursor(20, 220);
  gfx->print("Condition: ");
  gfx->print(data.condition);
}
```

### 2. Touch Button Interface

This example demonstrates creating interactive buttons:

```cpp
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Display setup code as shown earlier...

// Button definitions
struct Button {
  int x, y, w, h;
  uint16_t color;
  uint16_t textColor;
  String label;
  bool pressed;
};

Button buttons[3] = {
  {100, 100, 200, 80, RED, WHITE, "Button 1", false},
  {100, 200, 200, 80, GREEN, BLACK, "Button 2", false},
  {100, 300, 200, 80, BLUE, WHITE, "Button 3", false}
};

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  gfx->begin();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  gfx->fillScreen(BLACK);
  
  // Draw title
  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(20, 20);
  gfx->print("Touch Button Demo");
  
  // Draw buttons
  drawButtons();
}

void loop() {
  // In a real application, you would handle touch input here
  delay(1000);
  
  // Simulate button press for demo
  static int buttonIndex = 0;
  pressButton(buttonIndex);
  delay(500);
  releaseButton(buttonIndex);
  buttonIndex = (buttonIndex + 1) % 3;
}

void drawButtons() {
  for (int i = 0; i < 3; i++) {
    drawButton(buttons[i]);
  }
}

void drawButton(Button &btn) {
  gfx->fillRoundRect(btn.x, btn.y, btn.w, btn.h, 10, btn.color);
  gfx->drawRoundRect(btn.x, btn.y, btn.w, btn.h, 10, WHITE);
  
  gfx->setTextColor(btn.textColor);
  gfx->setTextSize(2);
  
  // Center text
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(btn.label, 0, 0, &x1, &y1, &w, &h);
  int textX = btn.x + (btn.w - w) / 2;
  int textY = btn.y + (btn.h - h) / 2 + h;
  
  gfx->setCursor(textX, textY);
  gfx->print(btn.label);
}

void pressButton(int index) {
  buttons[index].pressed = true;
  // Highlight pressed button
  gfx->drawRoundRect(buttons[index].x+2, buttons[index].y+2, 
                    buttons[index].w-4, buttons[index].h-4, 
                    8, WHITE);
  
  // Log button press
  Serial.print("Button pressed: ");
  Serial.println(buttons[index].label);
}

void releaseButton(int index) {
  buttons[index].pressed = false;
  // Restore button appearance
  gfx->drawRoundRect(buttons[index].x+2, buttons[index].y+2, 
                    buttons[index].w-4, buttons[index].h-4, 
                    8, buttons[index].color);
}
```

### 3. LVGL Dashboard Example

This example shows how to create a simple dashboard using LVGL:

```cpp
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

// Display setup code as shown earlier...

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[screenWidth * 10];

// Display flush callback
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
  
  lv_disp_flush_ready(disp);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  gfx->begin();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Initialize LVGL
  lv_init();
  
  // Initialize buffer
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, screenWidth * 10);
  
  // Initialize display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  
  // Create dashboard UI
  createDashboard();
}

void loop() {
  lv_timer_handler();
  delay(5);
}

void createDashboard() {
  // Create a tab view
  lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 50);
  
  // Add tabs
  lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "Dashboard");
  lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "Settings");
  lv_obj_t *tab3 = lv_tabview_add_tab(tabview, "Info");
  
  // Add content to Dashboard tab
  lv_obj_t *meter1 = lv_meter_create(tab1);
  lv_obj_set_size(meter1, 200, 200);
  lv_obj_align(meter1, LV_ALIGN_TOP_LEFT, 20, 20);
  
  // Add scale to the meter
  lv_meter_scale_t *scale = lv_meter_add_scale(meter1);
  lv_meter_set_scale_ticks(meter1, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(meter1, scale, 8, 4, 15, lv_color_black(), 10);
  lv_meter_set_scale_range(meter1, scale, 0, 100, 270, 135);
  
  // Add a data series
  lv_meter_indicator_t *indic = lv_meter_add_needle_line(meter1, scale, 4, lv_palette_main(LV_PALETTE_RED), -10);
  lv_meter_set_indicator_value(meter1, indic, 50);
  
  // Add a label
  lv_obj_t *label = lv_label_create(tab1);
  lv_label_set_text(label, "Temperature");
  lv_obj_align_to(label, meter1, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
  
  // Add a chart
  lv_obj_t *chart = lv_chart_create(tab1);
  lv_obj_set_size(chart, 300, 200);
  lv_obj_align(chart, LV_ALIGN_TOP_RIGHT, -20, 20);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  
  // Add a data series to the chart
  lv_chart_series_t *ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  
  // Set some dummy data
  for(int i = 0; i < 10; i++) {
    lv_chart_set_next_value(chart, ser, lv_rand(10, 90));
  }
  
  // Add a slider to Settings tab
  lv_obj_t *slider = lv_slider_create(tab2);
  lv_obj_set_width(slider, 300);
  lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 50);
  
  // Add info text to Info tab
  lv_obj_t *info = lv_label_create(tab3);
  lv_label_set_text(info, "ESP32-8048S070C\nDisplay Demo\nUsing LVGL v9.2.2");
  lv_obj_align(info, LV_ALIGN_CENTER, 0, 0);
}
```

These examples demonstrate different ways to utilize the ESP32-8048S070C display, from simple graphics to more complex LVGL-based interfaces. You can use them as starting points for your own projects.

## Comprehensive Troubleshooting Guide

### Display Issues

#### No Display Output

1. **Check Power Supply**:
   - Verify the ESP32 is receiving adequate power
   - Check for any power-related error messages in the serial monitor

2. **Verify Backlight**:
   - Confirm the backlight pin (TFT_BL) is correctly defined
   - Ensure the backlight is turned on in the code with `digitalWrite(TFT_BL, HIGH)`

3. **Check Pin Connections**:
   - Double-check all pin connections against the pin definitions
   - Look for loose connections or bent pins

4. **Initialization Sequence**:
   - Verify that `gfx->begin()` is called before any drawing operations
   - Check that the display initialization parameters match the documentation

#### Partial or Corrupted Display

1. **Timing Parameters**:
   - Ensure the hsync/vsync timing parameters match exactly as specified
   - Incorrect timing can result in partial display or scrolling issues

2. **GFX Instance Parameters**:
   - The 5th parameter in the GFX constructor should be `true` for proper operation
   - Example: `Arduino_RGB_Display(screenWidth, screenHeight, bus, 0, true, NULL, TFT_BL)`

3. **Buffer Issues**:
   - If using double buffering, ensure proper buffer management
   - Check for buffer overflows in your drawing code

#### Text Rendering Problems

1. **Text Position**:
   - Avoid placing text too close to the edges of the display
   - For text near edges, consider using a contrasting background rectangle

2. **Text Color**:
   - Ensure sufficient contrast between text color and background
   - Use `gfx->setTextColor(color)` before any text drawing operations

3. **Text Size**:
   - Very large text sizes may cause rendering issues
   - If text appears corrupted, try a different size or font

### Compilation Issues

1. **LVGL Assembly Errors**:
   - Verify the custom build script is correctly excluding assembly files
   - Check that all LVGL optimization flags are correctly set in platformio.ini

2. **Library Conflicts**:
   - Ensure you're using the GitHub version of Arduino_GFX library
   - Check for multiple versions of the same library in your project

3. **Missing Configuration Files**:
   - Verify that lv_conf.h exists in the include directory
   - Check that the LV_CONF_PATH build flag is correctly set

### Performance Issues

1. **Slow Rendering**:
   - Minimize the number of drawing operations in loops
   - Use hardware acceleration where available
   - Consider using LVGL's task scheduler for complex animations

2. **Memory Problems**:
   - Monitor memory usage through the serial monitor
   - Reduce buffer sizes if experiencing heap fragmentation
   - Use PSRAM if available for larger buffers

### Communication with Serial Monitor

If you're having trouble diagnosing issues, add debug statements to your code:

```cpp
Serial.println("Display initialization started");
gfx->begin();
Serial.println("Display initialization completed");
```

### Recovery Steps

If your display is completely non-functional:

1. Revert to the basic test pattern code provided in this documentation
2. Verify all hardware connections
3. Flash the ESP32 with the test code
4. Check serial output for any error messages
5. Gradually add back your custom code while testing at each step

## PlatformIO Configuration

### Manual Board Configuration

This project uses a direct manual configuration approach in the `platformio.ini` file, which eliminates the need for external Git submodules and simplifies the setup process.

Here's the complete configuration for the ESP32-8048S070C display:

```ini
[env:esp32-8048S070C]
platform = espressif32
board = esp32s3box
framework = arduino
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_build.flash_size = 16MB
board_build.partitions = default_16MB.csv

; Set CPU frequency
board_build.f_cpu = 240000000L

; Library dependencies
lib_deps =
    https://github.com/moononournation/Arduino_GFX.git
    lvgl/lvgl@^9.2.2

; Custom build script to handle LVGL configuration
extra_scripts = custom_build_script.py

; Build flags for LVGL configuration
build_flags =
    -D LV_USE_DRAW_SW_ASM=0
    -D LV_USE_VECTOR_GRAPHIC=0
    -D LV_USE_DRAW_SW_HELIUM=0
```

### Key Configuration Elements

1. **Board Selection**: Uses the ESP32-S3-Box as the base board configuration
2. **Memory Configuration**: Configured for optimal performance with the ESP32-S3
3. **Flash Settings**: Set to use the full 16MB flash capacity
4. **Libraries**: Includes the Arduino_GFX library and LVGL 9.2.2
5. **Custom Build Script**: Handles special LVGL configuration needs
6. **Build Flags**: Disables assembly optimizations that can cause issues with the ESP32-S3

### Custom Build Script

The `custom_build_script.py` file is used to modify the build process to exclude certain assembly files that may cause compilation issues:

```python
Import("env")

def modify_build_flags(env):
    print("Applying custom build configuration to exclude assembly files...")
    
    # Modify build flags to exclude assembly files
    env.Append(CPPDEFINES=[
        ("LV_USE_DRAW_SW_ASM", "0"),
        ("LV_USE_VECTOR_GRAPHIC", "0"),
        ("LV_USE_DRAW_SW_HELIUM", "0")
    ])
    
    # Add a build hook to exclude assembly files during compilation
    env.AddBuildMiddleware(lambda node: None if str(node).endswith(".S") else node, "*")
    
    print("Custom build configuration applied successfully.")

modify_build_flags(env)
```

This script ensures that problematic assembly files are excluded during compilation, which is necessary for compatibility with the ESP32-S3.

### Required Pin Configuration

The pin configuration must match the hardware connections as shown in the main.cpp file:

```cpp
// Pin definitions for ESP32-8048S070C
#define TFT_DE 41
#define TFT_VSYNC 40
#define TFT_HSYNC 39
#define TFT_PCLK 42
#define TFT_R0 14
#define TFT_R1 21
#define TFT_R2 47
#define TFT_R3 48
#define TFT_R4 45
#define TFT_G0 9
#define TFT_G1 46
#define TFT_G2 3
#define TFT_G3 8
#define TFT_G4 16
#define TFT_G5 1
#define TFT_B0 15
#define TFT_B1 7
#define TFT_B2 6
#define TFT_B3 5
#define TFT_B4 4
#define TFT_BL 2
```

### Special Considerations

1. **PSRAM Configuration**: The ESP32-S3 on this board has PSRAM which should be enabled for optimal performance with large displays:
   ```ini
   board_build.arduino.memory_type = qio_opi
   ```

2. **Assembly Optimizations**: Due to compatibility issues with the ESP32-S3, assembly optimizations in LVGL should be disabled as shown in your custom build script:
   ```ini
   -D LV_USE_DRAW_SW_ASM=0
   -D LV_USE_VECTOR_GRAPHIC=0
   -D LV_USE_DRAW_SW_HELIUM=0
   ```

3. **Version Differences**: If you experience display color issues, check which version of the board you have (1.1 or 1.3) as they have different GPIO-to-RGB mappings.
