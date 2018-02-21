Wifi pond
=========

A square of programmable RGB lights connected to a Wifi sniffer to assign
every decoded MAC address a flicker of light.

Hardware
--------

- [WEMOS D1 mini](https://www.aliexpress.com/item/D1-mini-Mini-NodeMcu-4M-bytes-Lua-WIFI-Internet-of-Things-development-board-based-ESP8266/32529101036.html) board
- An [8x8 array of WS2812B leds ](https://www.aliexpress.com/item/8x32-Pixel-256-Pixels-WS2812B-Digital-Flexible-LED-Panel-Individually-Addressable-Full-Dream-Color-DC5V/32776887881.html)
- A [beefy 5V 6A adapter](https://www.aliexpress.com/item/EU-US-UK-AU-Power-Supply-Adapter-Transformer-AC-110-240V-to-DC-5V-12V-24V/32776767537.html) to allow the LEDS to shine
- A frame to put it all in
- Some baking paper, to dissipate the light a bit
- A [2.5mm DC jack](https://www.aliexpress.com/item/20pcs-Connector-DC-Power-5-5mm-x-2-5mm-female-jack-socket-RF-COAXIAL-CCTV/32727861732.html) to connect up the power supply nicely

Connections
-----------

On the [WEMOS D1](https://wiki.wemos.cc/products:d1:d1_mini):
- Connected D2 to the RGB led DIN (Data In) line
- Connected GND to the ground of the 5V
- Connected 5V to 5V power supply
- Put simple push-button between 3.3V and D8. D8 has a build-in pull-down 10K resistor, so direct connection should not be a problem

On the LED array:
- Desolder one of the connectors, the data out one, and re-use it to connect up the WEMOS on the other end.
- Connect up the 5V and GND lines just to make sure enough amps can go to either side of the strip (probably pointless, but I liked the extra soldering)


Software
--------
- Arduino studio with
    - The Adafruit NeoPixel library
    - The ESP8266WiFi library
    - The [WEMOS D1 compiler and configuration setup](https://wiki.wemos.cc/tutorials:get_started:get_started_in_arduino)
