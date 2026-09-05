SKRPACE010 / LCSC C139797 - KiCad 10 footprint

Footprint:
  SW_SKRPACE010_C139797.kicad_mod

Use with symbol:
  Switch:SW_Push

Part:
  Manufacturer: ALPS Alpine
  MPN: SKRPACE010
  LCSC: C139797
  Function: SPST-NO momentary tactile pushbutton
  Body: 4.2 x 3.2 mm
  Height: 2.5 mm

Land pattern used:
  Manufacturer recommended land dimensions:
    overall X = 5.2 mm
    inner X gap = 3.1 mm
    overall Y = 2.8 mm
    inner Y gap = 1.5 mm

  Therefore each pad = 1.05 x 0.65 mm
  Pad centers = X +/-2.075 mm, Y +/-1.075 mm

Electrical mapping:
  The switch has four physical terminals in two internally-common pairs.
  This footprint intentionally uses duplicate KiCad pad numbers:
    left pair  = pad 1
    right pair = pad 2

  This lets it connect directly to KiCad's 2-pin Switch:SW_Push symbol.

Reset wiring:
  RESET/EN ---- SW ---- GND

No additional resistor is part of the switch footprint.

Syntax checks performed:
  - balanced S-expression parentheses
  - quoted strings checked
  - four SMD pads present
  - duplicate pad numbers 1,1,2,2 intentional
