# Component placement review - 20 October 2026

All 88 nonmechanical component references were inventoried by net, position and assembly status. The supply, reference, charge-pump, signal-filter, microphone and termination groups were reviewed against the circuits they serve. This review covers the saved design; it does not replace a physical fit or powered prototype test.

- C3/C7 headphone coupling capacitors are together beside the right-edge headphone jack. R10/R11 remain by the codec outputs.
- C86 is the touch supply bypass, approximately 4.9mm pad-to-pad from TS1 VDD, just above the flex passage. R50 is the touch reset pull-up below that passage. These serve the touch connector rather than the P4.
- C1/C2 are optional 220pF I2C filters, DNP. They are not supply decouplers and must remain unpopulated unless bus rise-time testing supports fitting them.
- U9, C94 and R49 moved to the left of the TFT slot. C95 moved to the connector's supply side. The passage between slot and connector is clear of those components. C87 is beside the lower connector end and C88 remains local bulk storage.
- Every fitted 100nF capacitor is listed in Checks/100nF-placement.csv with its net and geometric pad separation to a relevant device. Dedicated codec supply/reference capacitors are approximately 1.8-3.6mm from their pins. TFT C95 is approximately 4.6mm from the supply pins, on the connector side that leaves the flex path clear. U9 C94 is approximately 1.8mm from VCC. These distances are not measurements of trace length or loop inductance.
- C5/C6 sit at the filtered ES8388 rail near ferrite L1. The codec also has dedicated local bypasses C17/C33/C34 at its supply pins. C4 is local bulk storage for this filtered rail.
- PCM5102 charge-pump flying and negative-rail capacitors remain beside the IC. Its LDO bypass and output RC filter components remain in their functional groups.
- SD C77/C76 remain beside the socket supply. The pull-ups were moved slightly to keep the new SPI legends unobstructed; they are not high-frequency series terminations.
- U8 input/output capacitors remain close to the regulator pins, with its ground tab solidly connected to Bottom GND.
- R17/R6/R7/R8 are I2S series terminations near the module driver. R5 is near the codec's ADC output driver. Their placement near the source is intentional.
- Backlight gate resistor and gate pull-down remain beside Q3; the current-limiting resistor and local 5V reservoir stay with that circuit.
- I2C pull-ups stay near the module header. GPIO labels and connector locations were checked separately from supply-decoupling placement.

The original mounting-hole coordinates, board size and antenna keepout are retained. Exact touch ribbon pinout/contact side and final ribbon fold clearance still need confirmation with the actual display.
