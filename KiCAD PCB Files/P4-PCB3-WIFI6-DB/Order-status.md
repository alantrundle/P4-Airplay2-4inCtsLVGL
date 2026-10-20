# Ordering status - 20 October 2026

The user confirmed the purchased touch flex pin order: 1 VDD, 2 SDA, 3 SCL, 4 INT, 5 RST, 6 GND. It matches the carrier TS1 wiring exactly. No electrical change is required and the previous unknown-pin-order hold is resolved on that basis.

The supplied 40-pin ILI9488 PDF and supplier email remain the LCD wiring references. The similar LCDWiki ST7796 module confirms the shared TFT/SD SPI architecture, but has a different 48-pin LCD and reversed touch SDA/SCL pin order; it is not being used to override the user's screen pinout.

The exact touch controller model, cable contact-side orientation, socket mating and flex bend clearance have not been independently established by a matching manufacturer drawing or physical prototype. Match pin 1 and exposed contacts during assembly; do not substitute the similar LCDWiki cable pinout.

Final native ERC/DRC and connectivity checks pass under the project settings documented in Checks. This is a checked prototype design, not a powered functional validation. JLC stock/assembly orientation still need the order-preview check. Upload matching Gerbers, BOM and CPL together from this bundle.
