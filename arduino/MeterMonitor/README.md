A TM1637-based HeaterMeter client display.

Requires TM1637 and HeaterMeterClient libraries
* TM1637 - https://github.com/avishorp/TM1637
* HeaterMeterClient - [in this repository](https://github.com/CapnBry/HeaterMeter/tree/master/arduino/libraries/HeaterMeterClient)

Wiring (Wemos D1 Mini -> TM1637):
* 5V -> 5V (all displays)
* GND -> GND (all displays)
* D1 (GPIO5)  -> CLK (all displays)
* D2 (GPIO4)  -> DIO Probe 0 display
* D7 (GPIO13) -> DIO Probe 1 display
* D6 (GPIO12) -> DIO Probe 2 display
* D5 (GPIO14) -> DIO Probe 3 display

![MeterMaster Schematic Image](schematic.png)

## Parts List

* 1x Wemos D1 Mini or any ESP8266 variant with at least 5 usable GPIOs. [Amazon Affiliate Link](https://amzn.to/2ZjXiuq)
* 4x TM1637 7-segment display modules with **decimal** points not "clock". Warning! These are too small to fit the case below [Amazon Affiliate Link](https://amzn.to/3u6R1jK)
* Simple 3D printed case [Thingiverse](https://www.thingiverse.com/thing:4750046) for 0.56" LED modules
* If your LED modules flicker too much, this can be eliminated by adding a capacitor between 5V/GND. Two of my modules had some flickering so I added 3x 100uF/10V capacitors, but 1x 330uF would have worked too.

I would recommend searching eBay or Aliexpress for these parts due to the Amazon parts being 30% more expensive than they can be purchased from China.
