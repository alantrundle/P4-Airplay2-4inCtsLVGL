# Circuit review - 20 October 2026

## Changes

Both audio jacks are on Bottom, facing out of the right board edge beside the module sockets. Both R2 contacts connect to GND. Line-out is tip=L, R1=R, R2=GND, sleeve=GND. Headset is CTIA: tip=L, R1=R, R2=GND, sleeve=microphone. It accepts ordinary TRS headphones; a standalone TRS microphone uses a different pin arrangement.

U9, SN74LVC1G125DBVR, isolates LCD SDO from the shared SPI MISO whenever TFT_CS is high. This addresses the documented ILI9488 shared-bus risk. SD and TFT remain continuously powered and have separate chip selects. R49 holds TFT_CS high during startup. R50 and R51 hold touch and TFT reset high. Firmware must serialize SPI transactions and configure both chip selects high before initializing either device.

Q3 AO3400A is the only retained FET: pin 1 gate via R15=100 ohm from GPIO48, pin 2 source to GND, pin 3 drain to LED_K. R23=100k holds it off at startup. Higher PWM duty increases brightness. The panel specifies eight parallel LEDs and a 120mA maximum. R26 is now 22 ohm, 1%, 0.5W, 1210 (C2960863). Calculated current is about 86mA at 5V/3.1V LED voltage; at 5.25V, 2.9V LED voltage and minimum resistor tolerance it is 108mA, dissipating 0.254W. These calculations assume a regulated USB rail within that limit. C96 adds local 10uF on the backlight supply.

C94 provides local 100nF for U9; C95 adds local 100nF at the TFT supply; C86 is just above the touch ribbon passage, near the touch connector; R50 is below that passage. SD retains its local 100nF and 10uF. I2C pull-ups R1/R2 are closer to the Waveshare header. Regulator input/output capacitors are close to their pins and its ground tab connects solidly to Bottom GND. U2/U3 decoupling was checked against their supply pins; nearest 100nF pad separations are approximately 1.8-3.6mm. These are geometric pad distances, not measured loop inductance.

The module was shifted slightly left/down, the power LED lowered. I2C is on the left edge above the touch-ribbon slot; SPI is on the bottom edge beside J6 and GPIO is further along that bottom edge. Vertical JST labels identify signals and GPIOs. The module remains within the carrier outline. The four mounting holes are unchanged. Top uses a connected 3.3V fill with routed 5V; Bottom uses GND fill with signals. The module antenna keepout remains on both layers.

## Operating limits and firmware

Carrier ES8388 address is 0x11, separate from the module ES8311 at 0x18. Touch is expected to be an FT6236-family device at 0x38, subject to exact panel confirmation. Start I2C at 100kHz. Carrier 4.7k pull-ups in parallel with module 2.2k give approximately 1.5k, requiring roughly 2mA low-level sink current. Check attached expansion circuits for additional pull-ups and duplicate addresses.

Start SPI conservatively (10MHz) and raise speed only after testing the assembled display/card. The module's own SD interface is separate. GPIO24-27 are shared with module USB mux functions: configure the carrier's assigned GPIO/I2S functions and do not enable conflicting USB functions. Refer to GPIO-map.csv.

PCM5102 output filters and supply connections were checked against the manufacturer reference. Its line-out is for high impedance inputs (at least 1k, preferably 10k), not headphones. ES8388 headphone outputs retain 100uF capacitors and 22 ohm series resistance. With 32 ohm headphones this introduces approximately 4.5dB attenuation and a nominal 29.5Hz high-pass corner; ceramic capacitor DC bias can reduce capacitance and bass response. This is not a guarantee of flat response to 20Hz.

LD39150 is rated 1.5A, but 5V to 3.3V at 1.5A dissipates 2.55W. Continuous allowable current depends on actual copper cooling and ambient temperature. Backlight runs from 5V directly. Check temperatures and USB supply capacity on the prototype.

## Verification and physical checks

Checks/Electrical-audit.json records critical pin/net checks and schematic-to-PCB pad comparisons. DRC/ERC files record native KiCad results under the retained project settings. No powered hardware test has been performed. CTP pin order now matches the user confirmation; mating contact side and physical fit remain to be checked: see Order-status.md.

## References

- [PCM5102A datasheet](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf)
- [AO3400A datasheet](https://www.aosmd.com/sites/default/files/res/datasheets/AO3400A.pdf)
- [SN74LVC1G125 datasheet](https://www.ti.com/lit/ds/symlink/sn74lvc1g125.pdf)
- [LD39150 datasheet](https://www.st.com/resource/en/datasheet/ld39150.pdf)
- [Waveshare module documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-DB)
- [ILI9488 shared-MISO note](https://raw.githubusercontent.com/Bodmer/TFT_eSPI/master/User_Setup.h)
