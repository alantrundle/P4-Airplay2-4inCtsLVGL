# P4-WVM-ATSC6-V1 revision, 17 October 2026

Five horizontal, surface-mount JST-PH connectors now use 2.00 mm pitch. Their pin assignments are unchanged. The earlier 1.00 mm JST-SH mating cables do not fit this revision.

| Reference | Function | PCB anchor mm | LCSC / JLCPCB | Manufacturer part |
|---|---|---|---|---|
| J9 | I2C | 75, 145 | C265104 | S5B-PH-SM4-TB(LF)(SN) |
| J10 | SPI | 93, 145 | C265453 | S7B-PH-SM4-TB(LF)(SN) |
| J11 | GPIO | 115, 145 | C265121 | S8B-PH-SM4-TB(LF)(SN) |
| J12 | CONTROL | 55, 116 | C265104 | S5B-PH-SM4-TB(LF)(SN) |
| J13 | UART | 55, 89 | C265104 | S5B-PH-SM4-TB(LF)(SN) |

J12 and J13 face out of the same board edge as the microSD socket. J12 clears the nearby PCB slot and sits above J6 with space between the housings. J9, J10 and J11 face out of the bottom edge. JLC lists the [5-pin](https://jlcpcb.com/partdetail/JST-S5B_PH_SM4_TB_LF_SN/C265104), [7-pin](https://jlcpcb.com/partdetail/JST-S7B_PH_SM4_TB_LF_SN/C265453) and [8-pin](https://jlcpcb.com/partdetail/JST-S8B_PH_SM4_TB_LF_SN/C265121) parts as Extended. Order stock and prices require a new quote.

The detached pin legends and connector reference labels are removed from the board silkscreen. Each signal has a label aligned with its pin position. The bottom row has vertical labels on the exposed strip on side A, clear of the TFT outline. This keeps them visible after assembly rather than beneath the module or battery holder on side B. CONTROL and UART have labels beside their pads on side B. Functional titles are I2C, SPI, GPIO, CONTROL and UART.

Pins, starting at pin 1:

- J9: GND, 5V, 3.3V, GPIO7(SDA), GPIO8(SCL).
- J10: GND, 5V, 3.3V, GPIO2(SCK), GPIO3(MOSI), GPIO4(MISO), GPIO5(CS).
- J11: GND, 5V, 3.3V, GPIO13, GPIO22, GPIO34, GPIO49, GPIO50.
- J12: GND, 5V, 3.3V, ESP_EN, ESP_BOOT.
- J13: GND, 5V, 3.3V, U0TXD, U0RXD. These are 3.3 V UART levels; a true RS232 connection needs an external level converter.

## RTC and programming

J8 is now a 2.54 mm, three-pin right-angle header on side A, at 61, 79.5 mm, facing toward the side edge. Its pins are labelled directly: CELL, VBAT, 3V3. Pins 1-2 select the battery; pins 2-3 select 3.3 V. It remains manual/DNP, with no automatic-placement LCSC code, and needs a separate shunt.

J7 C6 programming is on side A at 69, 71 mm, outside the TFT body outline. Pins 1-4 are C6_BOOT, C6_RX, C6_TX, GND, labelled BOOT, RX, TX, GND beside the header. J7 remains manual/DNP. It is separated from the RTC selector. RESET moved to the top-edge clear area at 62, 67.5 mm. The coin-cell holder moved slightly inward to 100.3, 131.5 mm to clear the deeper JST housings. The P4 module has not moved.

## Audio

JP1 and its mic-selector silkscreen are removed. R48 now connects directly to MIC_BIAS_FILTERED, making the existing current-limited microphone bias permanently enabled. R47, R48, C89-C93 and their values remain unchanged. The ES8388 input still needs the appropriate differential input selection and microphone PGA settings in firmware. Firmware is unchanged.

J2 remains CTIA: tip left headphone, ring 1 right headphone, ring 2 ground, sleeve microphone. Three-contact stereo headphones remain compatible. J3 / PCM5102 remains line output: tip left, ring 1 right, ring 2 and sleeve ground.

## Power and manufacturing

5 V and main 3.3 V surface connections are 0.4 mm wide and connect locally into the inner power planes. Additional 5 V pour branches serve the relocated connectors. Switched SD power, filtered codec power and the RTC battery rail remain separate. Signal routing uses the outer layers and, where needed, the existing inner ground layer. No new signal routing crosses the inner power layer. The RF feed and antenna keepout are unchanged.

The project fabrication constraints and power net class match the previously checked manufacturing settings. Local libraries include the three new JST-PH footprints and right-angle RTC header.

Checks on the released revision:

- KiCad 10.0.7 DRC: 0 violations, 0 unconnected items, 0 schematic parity issues.
- ERC: 0 violations.
- Assembly BOM: 47 groups, 111 fitted parts per board, 555 for five boards before attrition.
- BOM and placement reference sets match without duplicates.
- AE1 printed copper, H1-H4 mechanical holes, and manual headers J7/J8 are excluded from assembly exports. JP1 no longer exists.
- Placement uses the installed JLC exporter transformations and persistent body-centre offsets. The PH connectors have the exporter's 180-degree package correction applied; do not replace those rotations with raw KiCad angles. Confirm supplier-model orientations in JLC's preview before purchase.
- Gerber ZIP: 11 layer files, four copper layers, both pastes, both masks, both silkscreens and outline, plus separate PTH and NPTH drill files.

Use the BOM, position file and Gerbers from this revision together. The previous manufacturing files describe different connectors and placements.
