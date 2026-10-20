# PCB3 verification - 20 October 2026

- Physical outline: 99.9 x 99.9 mm. Native Gerber job bounds, including the 0.05 mm outline stroke: 99.95 x 99.95 mm.
- Two copper layers, nominal 1.6 mm board, 35 um outer copper. No inner-layer fabrication files.
- KiCad 10.0.7 DRC: 0 violations, 0 unconnected items, 0 schematic parity mismatches after final zone refill.
- KiCad ERC: 0 reported violations.
- The source project's existing ignored-check settings are retained and listed in Checks/DRC.json and Checks/ERC.json. No new suppressions were added.
- 83 assembled component references, 35 grouped BOM lines, 83 placement rows. No duplicate designators, quantity mismatches or missing placement entries. Three additional references are DNP.
- U1 is customer fitted, DNP, with no LCSC code. Two 1x20 female through-hole sockets are separately listed for hand fitting. C1/C2 are DNP.
- Single Top 3.3V zone and single Bottom GND zone. 5V uses explicit tracks. Copper pieces are electrically joined with routing/bridges; no duplicate autorouter power fills remain.
- Rounded TFT/touch ribbon slots are retained. The module's lower mechanical edge is approximately 4.61 mm above the TFT glass outline. All four mounting holes retain their original positions.
- Carrier RF feed/antenna removed. Keep-out beneath the Waveshare radio/antenna area applies to both copper layers, tracks and vias.

## Assembly uploads

The touch pin order now matches the user confirmation. Check contact-side orientation and physical fit during assembly; see Order-status.md. Upload Gerbers.zip for fabrication, selecting two layers and 1.6 mm thickness. Upload BOM-JLC.csv and CPL-JLC.csv for assembly. All three exports share a lower-left origin. BOM-full.csv includes customer-fitted items. Project-local symbol and footprint libraries are included.

Adapt firmware to GPIO-map.csv. The external SD socket now shares the TFT SPI bus and has its own chip-select. It is continuously supplied from the carrier 3.3V rail. See Assembly-notes.md for power, connector pinouts and manual fitting details.

The checks verify the saved design and fabrication data, not a powered prototype. Socket/mating-pin height, LCD flex bending and enclosure/plug clearance still require a physical fit check. JLC stock and assembly orientation should be confirmed in the order preview; inventory has not been reserved. Existing component codes are retained where the circuit/package is unchanged.
