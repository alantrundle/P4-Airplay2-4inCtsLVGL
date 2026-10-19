# P4-Audio-TFT — 19 October 2026 release

The saved schematic and PCB are routed and pass the native KiCad 10.0.7 checks: zero DRC errors, zero DRC warnings, zero unconnected items, zero schematic/PCB parity issues, and zero ERC violations. The full-track DRC option was used. Existing check configuration was retained; no rule limits were relaxed and no new exclusions were added. Native reports and detailed audits are in validation/.

## Files for JLCPCB

- `P4-Audio-TFT.zip`: current four-layer Gerbers and separate plated/non-plated Excellon drill files.
- `bom.csv`: 62 assembly groups, 178 fitted parts per board; use for five assembled boards.
- `positions.csv`: the same 178 designators, with JLC rotation conventions and corrected DS1/TS1 connector body centres.
- The older production folder is archived separately; use only this release's files together.

The outline is 100 × 85.4 mm with the rounded C6 antenna cutout and FPC slots. The copper order is front signal / inner power / inner ground / back signal. The inner ground layer contains no signal tracks. The intended 1.6 mm stack is JLC04161H-3313. Impedance estimates and USB widths depend on that stack; confirm the stack in the fabrication order. USB uses approximately 0.1708 mm traces with a 0.12 mm gap. Ordinary new vias are 0.4 mm diameter with 0.2 mm drill.

## Assembly sourcing is blocked

All 64 selected purchased part codes were refreshed from JLCPCB on 19 October. The fitted BOM contains 34 Basic and 28 Extended groups. Three exceptions prevent a complete five-board assembly order:

| Reference | Part | LCSC | JLC stock |
|---|---|---|---:|
| U1 | ESP32-P4NRW32X | C54540373 | 0 |
| U8 | LD39150DT33-R | C155559 | 0 |
| U9 | ESP32-C6-MINI-1-H4 | C6553337 | 2; five needed |

The specified internal-antenna C6 module and quiet Waveshare regulator were retained. Do not substitute an external-antenna C6 or a different regulator without checking the mechanical design and pinout. Resolve these exact-part supplies or arrange JLC sourcing/consignment before ordering assembly. Stock and minimums are snapshots, not reservations. The workbook includes allocation estimates, minimums, loss allowances, prices and source links.

J7 (C6 programming) and J8 (right-angle RTC selector) are manual-fit headers and are excluded from the assembly CSVs. Mounting holes are also excluded. Supply one 2.54 mm shunt for J8 separately. Its pins are CELL / VBAT / 3V3: fit one shunt to the required source. The RTC coin cell itself is not included in the assembly BOM.

## Routing and prototype qualification

The USB pair is approximately 95.897 mm per leg, matched within 0.001 mm, entirely on the back layer without vias. The six P4-to-C6 lines are approximately 67.90–68.22 mm including nominal via barrels, with maximum clock-relative mismatch below 0.23 mm. The six SD-card lines are approximately 136.30–136.38 mm, matched within 0.08 mm. These measurements do not replace signal-integrity simulation or hardware tests.

The SD-card routes are long for this fixed connector layout and exceed Espressif's preferred short-route guidance. Begin prototype SD-card testing at a conservative clock (for example 20 MHz), then test the intended card/clock/drive settings before relying on faster modes. Neither SD-card nor P4/C6 maximum-speed operation has been bench-qualified.

Power taps were widened to 0.4 mm where clearance allowed; short pin escapes remain narrower. Fifty-three angled local connections were made horizontal/vertical/45-degree, and 52 vias were moved outside small component lands. Some inherited vias remain in SMD/thermal ground lands; the copper audit records them for assembly DFM review and any tenting/filling requirements. Ground and power zones were refilled before the final checks.

Keep the C6 antenna cutout and all-layer copper keepout. Do not add copper or a ground via under the antenna. The C6 header is BOOT / RX / TX / GND. J13 is a 3.3 V UART breakout, not a bipolar RS-232 electrical interface. GPIO labels and the SPI/I2C pin assignments were checked against the actual PCB pads. The PCM5102 output is line-level; the ES8388 socket is the CTIA headphone/microphone connection with fixed microphone bias.

This is a checked prototype layout and manufacturing file set, not a claim of tested hardware. Check supply loading against the selected USB current limit during bring-up. No fabrication order or assembly purchase has been placed.
