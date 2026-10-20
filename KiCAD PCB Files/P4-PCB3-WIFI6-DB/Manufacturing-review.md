# Final circuit and manufacturing review - 20 October 2026

## Result

The saved design passes native KiCad ERC and DRC with 0 reported violations, 0 unconnected PCB items and 0 schematic/PCB parity mismatches. The retained project ignored-check settings are recorded in the reports; no new suppressions were added. The module/peripheral GPIO audit passes 132 explicit assertions. These results verify the design files under those settings, not operation of manufactured hardware.

The user has confirmed the six-pin touch order, and it matches TS1 exactly. The 40-pin TFT interface follows the supplied drawing and supplier's DC/SCLK correction. Contact-side orientation and physical flex fit still need an assembly check. See Order-status.md.

## TFT and SD circuit

- LCD pin10 is DC/GPIO33 and pin11 is SPI clock/GPIO31. MOSI is GPIO32; MISO returns on GPIO30. The mode pins select four-wire SPI.
- TFT CS46 and SD CS49 are separate. TFT reset47 has a pull-up; backlight48 drives the correctly mapped N-channel low-side FET through its gate resistor and gate pull-down.
- TFT SDO passes through U9, enabled only by active-low TFT CS. SD MISO and the J10 MISO breakout share the buffered return net. This addresses LCD MISO bus contention while the card is selected.
- SD is wired for SPI, continuously supplied from carrier 3.3V, and retains local 100nF/10uF and appropriate pull-ups. DAT1/DAT2 are not being treated as active four-bit SDMMC data lines. SW1 enables the separate card-detect signal on GPIO51.
- The module's onboard card slot uses separate SDMMC pins and is not electrically the same card socket.
- Firmware must share one SPI bus, serialize transfers, deselect both devices during initialization, initialize the external card in SPI mode and select the correct TFT pixel/data format. Initial SD clock must be <=400kHz. Speed and signal integrity need validation on the prototype before selecting a high transfer rate.

## PCB pass

- Two copper layers; nominal board thickness 1.6mm. Physical outline 99.9 x 99.9mm; Gerber job bounds include outline stroke and are 99.95 x 99.95mm. Rounded ribbon cutouts remain closed under native DRC. All four mounting-hole positions remain original.
- Single Top 3.3V fill, single Bottom GND fill, explicit 5V tracks. No duplicated autorouter power fills or inner-layer fabrication files. Radio antenna copper/track/via keepout remains on both layers.
- All nets are connected after zone refill. Native checks found no reported copper shorts/clearance faults, dangling tracks/vias, solder-mask bridges, courtyard overlaps or silkscreen faults in the final saved board. Existing ignored checks are listed in Checks/DRC.json.
- Minimum signal track width is 0.12mm; minimum via drill is 0.2mm. Power tracks use the existing wider routing. These features require the selected manufacturer's applicable process options; the final upload/DFM remains the manufacturer's check.
- The TFT passage between its slot and FPC is clear of U9/C94/R49/C95. Touch C86 moved above its ribbon passage and R50 below. Component-pad bounding boxes were checked against both corridor rectangles, with no intersections. Component body height, actual flex bend and assembly tolerances still need a physical fit check.
- C3/C7 are grouped near the right-edge headphone jack. All fitted 100nF capacitors were reviewed by net; placement details are in Placement-review.md and Checks/100nF-placement.csv. C86 is now approximately 4.9mm pad-to-pad from touch VDD, balancing decoupling proximity against flex clearance. C95 is approximately 4.6mm from TFT VDD, outside the TFT flex path.
- Jack icons are on side A near the right-edge openings, offset from the jack locating holes. Both jacks remain on side B. Connector pin legends remain on side B. JST physical pad order is documented in GPIO-report.md.
- BOM and placement references are matched without duplicates. U1 and optional I2C capacitors C1/C2 remain DNP; module LCSC number is blank. Female module sockets remain separately listed for manual fitting. JLC inventory is not reserved and final assembly rotations/contact variants still require review in its upload preview.

## USB power protection

The official Waveshare schematic places LTVS16H5.0FT5G TVS protection on USB0_5V and an AO3401 with MMDT3906DW control in the USB0_5V-to-VCC_5V path, providing reverse-current blocking. VSYS header pin39 supplies this carrier from VCC_5V. There is no explicit 500mA overcurrent limiter or series fuse shown in that USB path. The module's buck regulator current capability is not a USB input current limit.

Per the user's latest instruction, no additional carrier fuse/limiter was added. The existing module circuit addresses transients/backfeeding; it does not establish complete protection of a connected PC against every short or overload. A limiter on the carrier would cover only the carrier branch, not the module's own load. Total input-current protection would need to be upstream of the module USB input.

## Release limitations

Touch pin order is user-confirmed; connector contact side and physical flex fit have not been independently verified. This design has not been powered or tested for SD/display simultaneous operation, audio recording/playback, regulator temperature, USB load or radio performance. The LDO 1.5A rating does not establish safe continuous thermal operation at that current. Existing headphone attenuation/bass limitations are in Circuit-review.md. Gerbers, BOM and CPL must be uploaded together from this revision because they share the same origin and placements.

Sources: [Waveshare official schematic](https://github.com/waveshareteam/ESP32-P4-WIFI6-DB/blob/main/hardware/ESP32-P4-WIFI6-DB.pdf), [Waveshare GPIO reference](https://docs.waveshare.com/ESP32-P4-WIFI6-DB), and the user-supplied ILI9488 PDF/email. See GPIO-report.md and Circuit-review.md for peripheral datasheets.
