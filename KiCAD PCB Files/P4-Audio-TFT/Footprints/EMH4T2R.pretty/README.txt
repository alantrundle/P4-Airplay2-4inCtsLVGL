EMH4T2R_EMT6 KiCad footprint
================================

Part: ROHM EMH4T2R
Package: EMT6 / SOT-563 / SC-107C
LCSC: C3588692

Pin numbering (top view, pin-1 marker at upper-left in the footprint):
1 = Emitter 1
2 = Input/Base 1 (internal 10 kΩ series resistor)
3 = Collector 1
4 = Emitter 2
5 = Input/Base 2 (internal 10 kΩ series resistor)
6 = Collector 2

Footprint pads:
- row pitch: 0.50 mm
- pad centres: X = ±0.70 mm
- pad size: 0.45 x 0.30 mm
- package drawing/Fab nominal outline represented as 1.30 x 1.70 mm.
- 3D model: ${KICAD9_3DMODEL_DIR}/Package_TO_SOT_SMD.3dshapes/SOT-563.step

Installation:
1. Copy EMH4T2R.pretty into your project folder (or KiCad personal footprint folder).
2. Preferences > Manage Footprint Libraries > Project Specific Libraries.
3. Add existing library and select EMH4T2R.pretty.
4. Assign footprint: EMH4T2R:EMH4T2R_EMT6
5. Open Footprint Editor / 3D Viewer. If the standard KiCad 3D packages are installed,
   the SOT-563 STEP model will display automatically.

Validation performed:
- balanced KiCad S-expression syntax
- all six pads present exactly once
- pad sequence 1..6 checked
- 3D model path present
