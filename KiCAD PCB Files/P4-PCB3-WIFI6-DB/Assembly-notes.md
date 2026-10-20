# PCB3 - Waveshare ESP32-P4-WIFI6-DB carrier

This is the PCB3 carrier revision. Read Order-status.md before ordering and Circuit-review.md for the latest circuit changes and operating limits.

## Mechanical arrangement

- Main PCB: 99.9 x 99.9 mm (below the 100 x 100 mm maximum), 1.6 mm nominal thickness, two copper layers.
- TFT remains on the front (Top) side, with its existing glass outline and rounded ribbon cut-outs.
- Waveshare ESP32-P4-WIFI6-DB fits on the same side, horizontally along the upper edge, USB-C facing left.
- Fit two 1 x 20, 2.54 mm pitch female sockets, with 17.78 mm row spacing. Through-hole sockets are the selected fallback for hand soldering. No verified low-profile SMT part was selected. BOOMELE 2.54-1*20PFemale (C50984) is the listed standard-height socket reference; its advertised 3.3 mm dimension is the solder-tail length, not its housing height. The footprint uses Pico-style pin numbering: 1-20 left to right on the lower row, 21-40 right to left on the upper row, viewed from the top.
- U1 represents the complete customer-fitted module/socket assembly and is marked DNP for JLCPCB. Its LCSC code is intentionally blank. Fit both socket strips and the module after assembly. Use the DB version in the supplied link, not the older WIFI6 board or solder-down module.
- Allow clearance for mating socket height and components on the module underside. The mechanical envelope follows Waveshare's drawing; a physical module/display fit check is still required before enclosure manufacture.

## Power and layers

5 V comes from module VSYS, pin 39, and feeds the retained LD39150DT33-R regulator. The carrier's 3.3 V regulator output is separate from the module's 3V3 pin; pin 36 is unconnected. VBUS pin 40 is also unconnected. Do not apply a second 5 V source to the carrier expansion ports while USB is connected without suitable external isolation.

Top copper carries power and signals; Bottom copper is predominantly ground with signal routing. No inner copper is used. A copper/track/via keep-out is provided beneath the module's radio/antenna region. The old carrier RF antenna and matching connection are removed.

The LD39150 is rated at 1.5 A, but this does not guarantee that the assembled board can continuously dissipate (5 - 3.3) x 1.5 = 2.55 W. Check regulator temperature under the actual display/audio load. USB source capacity also limits the available current.

## Retained circuits and firmware changes

PCM5102 and ES8388 retain separate audio data/bus connections. The ES8388 series resistors are near the new module header at the driving end; its ADC-output resistor remains near the codec. All GPIO assignments have changed from the solder-down-module revision: use GPIO-map.csv and Module-header-pinout.csv when adapting firmware.

The external SD socket now operates in SPI mode, sharing TFT clock, MOSI and MISO, with a separate SD chip-select. Serialize access with a shared SPI bus driver, keep both chip-selects high during initialization, and initialize the SD card in SPI mode. The SD socket remains powered with the carrier's 3.3 V rail, preventing powered TFT bus signals from back-feeding an unpowered card. The SD clock pull-up and the old SD supply-switch transistor/resistors are removed; the 10 kOhm data/chip-select pull-ups remain. The module's own TF socket is a separate interface.

I2C and touch remain connected. C1 and C2 (optional 220 pF bus capacitors) are DNP to avoid unnecessary bus capacitance. Carrier pull-ups remain; the module has its own I2C devices and pull-ups, so check address assignments and configured bus speed in firmware.

J9: GND, 5V, 3V3, SDA, SCL.
J10: GND, 5V, 3V3, SPI SCK, SPI MOSI, SPI MISO, GPIO2 (available for expansion chip-select).
J11: GND, 5V, 3V3, GPIO3, GPIO24, GPIO25, GPIO52, GPIO50.

The old solder-down P4/C6 module, carrier antenna, USB-C input, host socket, CH340/programming circuitry, reset/power controls, RTC holder/selector and UART/control connectors are removed. SW1 remains as the SD-detect enable switch.

## Manufacturer references

- https://jlcpcb.com/partdetail/51996-2_54_1_20P/C50984
- https://docs.waveshare.com/ESP32-P4-WIFI6-DB
- https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/blob/main/hardware/ESP32-P4-WIFI6-DB.pdf


## Connector placement update - 20 October 2026

J9 I2C is on Bottom at the left edge above the touch-ribbon cutout. J10 SPI and J11 GPIO are on Bottom at the bottom edge, SPI nearest the SD socket. J3 line-out and J2 CTIA headset are on Bottom at the right edge, facing outwards. Mounting-hole positions are unchanged. The module courtyard describes its mechanical envelope; the separate two-layer antenna copper/routing keepout remains. A 9-11mm module standoff has not been demonstrated to remove the antenna-clearance requirement.

## Final verification and jack markings

Read GPIO-report.md for all 40 module header pins, peripheral GPIOs and JST pad order. Manufacturing-review.md records the final circuit/PCB checks and USB protection scope. The headphone and line-output icons are on side A by the right-edge jack openings. C86 is above and R50 below the touch flex path. No additional USB current limiter was added, per the final user instruction. The touch pin order is now user-confirmed; connector contact-side orientation and physical fit remain assembly checks.

## Bottom-only assembly selection

Upload the matching BOM-JLC.csv and CPL-JLC.csv together. Both jacks J2/J3 are included as C22459515. D1 and SW1 are excluded from both upload files and listed as Manual fit in BOM-full.csv; buy them separately. The assembly list contains 83 references in 35 BOM groups, all on Bottom. U1 and optional C1/C2 remain DNP. The full BOM also lists the two manually fitted module sockets.

Live JLC library classification was checked for all 35 fitted part groups: 22 Basic and 13 Extended. Common capacitors, small resistors and AO3400A are Basic. Extended selections retain the exact connector footprints, audio IC functions, LDO requirements, buffer, low-resistance 1A ferrite and 0.5W backlight resistor. No verified suitable Basic replacement was found for these. Public prices are indicative; no lowest-price-across-all-catalogue or stock-reservation guarantee is made. Details and source URLs are in Checks/BOM-selection.json.
