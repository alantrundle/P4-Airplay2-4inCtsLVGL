# P4 module and GPIO verification

Reviewed 20 October 2026 against the saved PCB3 schematic/PCB and the Waveshare ESP32-P4-WIFI6-DB 40-pin header schematic and pinout.

**Result:** all 40 U1 header pad assignments match the official DB header map. The six requested interfaces have valid GPIO assignments and the ES8388 has both playback and capture data connections. No carrier GPIO allocation collision was found. This is a connectivity/capability verification, not a powered functional test. The touch pin order matches the user-confirmed mapping: 1 VDD, 2 SDA, 3 SCL, 4 INT, 5 RST, 6 GND. Contact-side orientation and physical fit have not been independently tested.

## GPIO allocation

Directions below are relative to the P4, with the P4 acting as SPI/I2S master. Physical pin numbers mean U1 carrier footprint pads, not ESP32 package pin numbers or the generic GPxx labels inside the Pico-style symbol on the Waveshare schematic.

| P4 GPIO | U1 pin | Function | Direction | Connection |
|---:|---:|---|---|---|
| 2 | 15 | Expansion CS | Out / configurable | J10 pin 7; free chip-select, not SD_CS |
| 3 | 14 | Expansion GPIO | Configurable | J11 pin 4 |
| 4 | 12 | Touch RESET | Out | TS1 pin 5; R50 pull-up |
| 5 | 11 | Touch INT | In | TS1 pin 4 |
| 7 | 17 | I2C SDA | Bidirectional OD | TS1 pin 2, U3 pin 27, J9 pin 4; shared with module I2C |
| 8 | 16 | I2C SCL | Out OD | TS1 pin 3, U3 pin 28, J9 pin 5; shared with module I2C |
| 20 | 35 | ES8388 MCLK | Out | R17 33R -> U3 pin 1 |
| 21 | 34 | ES8388 BCLK | Out | R6 33R -> U3 pin 5 |
| 22 | 32 | ES8388 LRCK / WS | Out | R8 33R -> U3 pin 7 |
| 23 | 31 | ES8388 playback data | Out | R7 33R -> U3 pin 6 DSDIN (P4 DOUT) |
| 24 | 19 | Expansion GPIO | Configurable | J11 pin 5; alternative USB1 function must remain disabled |
| 25 | 20 | Expansion GPIO | Configurable | J11 pin 6; alternative USB1 function must remain disabled |
| 26 | 29 | ES8388 microphone data | In | U3 pin 8 ASDOUT -> R5 33R -> P4 DIN; alternative USB1 function disabled |
| 27 | 27 | PCM5102 BCLK | Out | U2 pin 13; alternative USB1 function disabled |
| 28 | 7 | PCM5102 playback data | Out | U2 pin 14 DIN |
| 29 | 6 | PCM5102 LRCK / WS | Out | U2 pin 15 |
| 30 | 5 | Shared SPI MISO | In | J6 pin 7 and U9 pin 4; J10 pin 6. TFT SDO reaches U9 pin 2 |
| 31 | 4 | Shared SPI SCLK | Out | DS1 pin 11, J6 pin 5, J10 pin 4 |
| 32 | 26 | Shared SPI MOSI | Out | DS1 pin 13, J6 pin 3, J10 pin 5 |
| 33 | 25 | TFT DC / A0 | Out | DS1 pin 10 |
| 46 | 24 | TFT CS active low | Out | DS1 pin 9 and U9 pin 1 /OE; R49 pull-up |
| 47 | 22 | TFT RESET | Out | DS1 pin 15; R51 pull-up |
| 48 | 21 | TFT backlight PWM | Out | R15 100R -> Q3 gate; R23 gate pull-down |
| 49 | 10 | External SD CS active low | Out | J6 pin 2 DAT3/CS |
| 50 | 9 | Expansion GPIO | Configurable | J11 pin 8 |
| 51 | 2 | External SD detect | In | SW1 pin 1 -> switched card-detect circuit; enable SW1, active low |
| 52 | 1 | Expansion GPIO | Configurable | J11 pin 7 |

## ES8388: headphone playback and headset microphone

Playback: P4 GPIO23 (I2S DOUT) -> R7 -> ES8388 DSDIN pin 6 -> DAC/headphone outputs -> R10/R11 and C3/C7 -> J2 tip/first ring. Capture: J2 sleeve (CTIA microphone) -> C90 -> ES8388 LIN1 pin 24; the reference input RIN1 pin 23 is AC-grounded through C91. Configure the codec input mux for the intended LIN1/RIN1 differential path and set microphone gain. The ADC sends samples from ASDOUT pin 8 through R5 to GPIO26 (P4 I2S DIN). R47/R48/C89 provide the fitted external microphone bias/filter network.

The microphone needs the P4 **I2S input**, while ASDOUT is the ES8388 **I2S output**. Both names describe the same capture wire from different ends. Keep GPIO26 as an input; GPIO23 is the playback output. The codec is an I2S slave with MCLK20, BCLK21 and WS22 supplied by the P4. Configure standard Philips I2S, matching ADC/DAC word lengths and a shared sample rate. A practical initial setting is 48kHz, 32-bit stereo slots, BCLK=3.072MHz, MCLK=12.288MHz (256fs), with mono microphone samples extracted from the selected slot.

ES8388 CE pin 26 is high, selecting 7-bit I2C address **0x11**. It does not use the module onboard ES8311 address 0x18 or its default GPIO9-13 audio wiring. Initialize the carrier ES8388 explicitly; an unchanged Waveshare audio example will target the onboard codec.

J2 is CTIA: tip=left, ring1=right, ring2=GND, sleeve=microphone. Standard TRS headphones are supported; a standalone TRS microphone uses a different wiring scheme. Headphone attenuation and coupling-capacitor limits are described in Circuit-review.md.

## PCM5102 line-out

Dedicated playback bus: GPIO27=BCLK, GPIO28=DIN, GPIO29=LRCK. Hardware straps select Philips I2S; SCK is grounded so the DAC uses its BCLK PLL. No fourth MCLK GPIO is required. U2 outputs feed J3 through the fitted output filters. J3 tip=left, ring1=right, ring2/sleeve=GND; use a line-level load.

The P4 provides three I2S controllers. Use one controller in full duplex for ES8388 and another in TX mode for PCM5102; the present separate clock wires support this arrangement. Playing the same stream through both DACs requires firmware to feed both outputs. Independent controllers are not a guarantee of sample-exact analog phase alignment. A third controller remains available for the onboard codec if firmware deliberately enables it.

## TFT and external SD

Both share SCLK31, MOSI32 and MISO30. TFT has CS46, DC33, RESET47 and backlight PWM48. SD has its own CS49 and card detect51. DS1 LCD pin 10=DC and pin 11=SCLK follow the supplier email correction. IM2/IM1/IM0 are high for four-wire SPI. The buffer U9 enables TFT SDO onto MISO only while TFT CS is low, preventing the TFT return signal from fighting the SD return signal when deselected. J10 shares this same bus.

Keep both CS lines high before initialization, initialize SD in SPI mode, and serialize transfers with one shared SPI-bus driver. Start SD initialization at <=400kHz and increase transfer speed only after successful initialization. Use a conservative display/card transfer clock for initial testing; do not treat ILI9488 as an SDMMC device. GPIO2 on J10 can select an additional SPI device; it is separate from SD CS49. Both devices remain powered. Backlight PWM high means brighter; startup gate pull-down leaves it off. Enable SW1 for card-detect reporting; otherwise GPIO51 requires an internal pull-up and does not indicate card insertion.

## Touch and I2C

TS1 carrier wiring is pin1=3.3V, pin2=SDA7, pin3=SCL8, pin4=INT5, pin5=RESET4, pin6=GND. The GPIO choices are available and distinct from the TFT/SD/audio pins. The user has confirmed this pin order for the purchased touch flex. This is user-provided confirmation rather than independent controller identification. Its pin order now matches the user confirmation. Connector contact-side orientation still needs checking against the actual cable. The assumed FT6236-family device is expected at 0x38; confirm by hardware identification/I2C scan.

GPIO7/8 share the module I2C bus. Carrier ES8388 at 0x11 does not collide with module ES8311 at 0x18. Any connected module DSI touch, backlight or camera devices also use this bus and need distinct addresses. Carrier C1/C2 are DNP I2C filters. Start at 100kHz; do not add excessive parallel pull-ups or configure GPIO7/8 as conflicting push-pull signals.

## Expansion connector pin order

Numbers are actual footprint pad numbers. Cable mating-face views may appear mirrored; follow the PCB pin-1 mark and pad numbers. All GPIO signals use 3.3V logic. The 5V pins are power only.

| Connector | Pins in numerical order |
|---|---|
| J9 I2C | 1 GND; 2 5V; 3 carrier 3.3V; 4 SDA/GPIO7; 5 SCL/GPIO8 |
| J10 SPI | 1 GND; 2 5V; 3 carrier 3.3V; 4 SCK/GPIO31; 5 MOSI/GPIO32; 6 MISO/GPIO30; 7 spare CS/GPIO2 |
| J11 GPIO | 1 GND; 2 5V; 3 carrier 3.3V; 4 GPIO3; 5 GPIO24; 6 GPIO25; 7 GPIO52; 8 GPIO50 |

Six otherwise unassigned GPIOs are broken out: **2, 3, 24, 25, 50, 52**. GPIO7/8 and GPIO30/31/32 are already shared with onboard carrier circuits, not spare independent pins.

## Module onboard resources and conflicts

The selected carrier pins avoid the module radio SDIO, onboard SDMMC, UART programming and boot straps. Module onboard audio GPIO9-13 is separate from carrier audio. GPIO24/25/26/27 have alternative USB1/USB PHY functions: leave those functions disabled while using J11 GPIO24/25, ES8388 capture26 and PCM BCLK27. The onboard USB-to-UART programming connector uses its separate UART circuit and is not removed by using these carrier assignments. Do not blindly enable a BSP USB configuration that claims these pins.

## Full U1 physical pin map

Pin1 is the GPIO52 end of the first row, at the module USB-C end; pin20 is the opposite end of that row. Pin40 is VBUS at the USB-C end of the other row; pin21 is at its opposite end. On this carrier the USB-C end faces left. The footprint numbers run 1->20 along one row and 40->21 along the other. Do not rotate the module 180 degrees.

| U1 pin | Official module function | Carrier net |
|---:|---|---|
| 1 | GPIO52 | GPIO52 |
| 2 | GPIO51 | SD_DETECT |
| 3 | GND | GND |
| 4 | GPIO31 | TFT_SCK |
| 5 | GPIO30 | TFT_MISO |
| 6 | GPIO29 | PCM_LRCK |
| 7 | GPIO28 | PCM_DATA |
| 8 | GND | GND |
| 9 | GPIO50 | GPIO50 |
| 10 | GPIO49 | SD_CS |
| 11 | GPIO5 | TCH_INT |
| 12 | GPIO4 | TCH_RST |
| 13 | GND | GND |
| 14 | GPIO3 | GPIO3 |
| 15 | GPIO2 | GPIO2 |
| 16 | GPIO8 | I2C_SCL |
| 17 | GPIO7 | I2C_SDA |
| 18 | GND | GND |
| 19 | GPIO24 | GPIO24 |
| 20 | GPIO25 | GPIO25 |
| 21 | GPIO48 | TFT_BL |
| 22 | GPIO47 | TFT_RESET |
| 23 | GND | GND |
| 24 | GPIO46 | TFT_CS |
| 25 | GPIO33 | TFT_DC |
| 26 | GPIO32 | TFT_MOSI |
| 27 | GPIO27 | PCM_BCLK |
| 28 | GND | GND |
| 29 | GPIO26 | I2S_ASDOUT |
| 30 | RUN | unconnected-(U1-RUN-Pad30) |
| 31 | GPIO23 | I2S_DSDIN |
| 32 | GPIO22 | I2S_LRCK |
| 33 | GND | GND |
| 34 | GPIO21 | I2S_BCLK |
| 35 | GPIO20 | I2S_MCLK |
| 36 | 3V3 | unconnected-(U1-3V3-Pad36) |
| 37 | EN | unconnected-(U1-EN-Pad37) |
| 38 | GND | GND |
| 39 | VSYS | MODULE_5V |
| 40 | VBUS | unconnected-(U1-VBUS-Pad40) |

VSYS pin39 supplies carrier 5V and U8 generates the carrier 3.3V. Module 3V3 pin36, VBUS40, EN37 and RUN30 are intentionally not connected by this carrier. The module regulator and carrier regulator outputs are not tied together. All eight module ground pins are connected.

## Evidence and limits

The automated audit checked 132 explicit pin/net assertions against both schematic and PCB, including every module header pin, all three JST headers, peripheral data/clock pins, audio series resistors and headset microphone coupling. Checks/GPIO-audit.json records the result. Existing native DRC/ERC reports cover their documented settings. No powered display, SD, audio recording or RF test has been performed. See Order-status.md for the user-confirmed touch pin order and remaining physical checks.

Primary references: [Waveshare pinout and onboard allocation](https://docs.waveshare.com/ESP32-P4-WIFI6-DB), [Waveshare official schematic](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/blob/main/hardware/ESP32-P4-WIFI6-DB.pdf), [Espressif P4 I2S documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/i2s.html), [PCM5102A datasheet](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf), [ES8388 manufacturer datasheet](https://datasheet.lcsc.com/datasheet/pdf/b5b69a0d4875954a40e8f8a3f42483af.pdf?productCode=C365736).
