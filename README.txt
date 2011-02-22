HeaterMeter Instructions

Web Site 
http://192.168.1.252/

Booting
When plugged in, HeaterMeter will connect to your wireless network.  This process will take 1-2 minutes as the security keys are calculated.  During this time, HeaterMeter will display "Connecting to (networkname)".  To prevent HeterMeter from attenpting to connect to the network, use the following procedure
-- Start with power off
-- Press and hold any button
-- Plug in the HeaterMeter
-- Once the display lights up, continue to hold the button for 2-3 seconds
-- Release the button to advance to the Home Status Screen

Home Status Screen
While on the home status screen, the top line of the LCD contains the pit temperature and fan speed.  The bottom line rotates between active food probes and the HeaterMeter internal temperature probe.  While in Automatic Control Mode, the Up and Down buttons will quickly scroll the bottom line between probes without havting to wait for them to rotate automatically.

Press the Right button to enter the menu system.  Press the Left button to manually enter "Lid Open" mode.

Menu System
Press the Right button to advance through the menus.  The Left button will take to back to the Home Status Screen, and the Up and Down buttons to change the options.

Set Point - The temperature that will be maintained by the automatic control mode.  Changing the set point will put your HeaterMeter into automatic control mode.
Manual Fan Mode - Allows you to set the fan speed manually, rather than having HeaterMeter maintain temperature and adjust the fan speed as necessary.  While operating in automatic fan mode, the fan speed on the Home Status Screen is contained in brackets [   ].  When in manual fan mode, the fan speed is contained between v's and can be adjusted upsing the Up (+5%), Down (-5%), and Left (-1%) buttons.
Max Fan Speed - By default, HeaterMeter will adjust the fan speed between 0% and 100%.  If you'd like to limit the maximum fan speed which is set automatically, it can be done here.  This can be used if a large amount of ash has accumulated in the grill and the fan is blowing it to grill area. 
Probe Name - Set the probe name here by using the Up and Down buttons.  Scroll the cursor all the way off to the left to cancel editing, or all the way off to the right to confim the edit.
Probe Offset - Calibrate your probes by putting them in boiling water and using probe offset to set them to read 212F.
Lid Open Offset - While temperature is being automatically maintained, if the temperature drops this many degrees, HeaterMeter will automatically enter Lid Open mode.
Lid Open Duration - The duration of the manual or automatic Lid Open delay.
Reset Config - Reset HeaterMeter to its default configuration.

Lid Open Mode
When opening the lid on the grill, the temperature wiil drop and HeaterMeter will ramp the fan speed up to compensate.  This will create a temperature overshoot once the lid is closed again.  To avoid this, when HeaterMeter detects that temperature drop when the lid has opened, it will stop the fan and suspend fan speed control.  Lid Open mode will continue for a preset time OR until the temerature has returned to the set point.  While in Lid Open mode, the Home Status Screen will show LidXXX where the fan speed normally is.  This timer will count down the time remaining in Lid Open mode.  The duration of this mode or the temperature drop trigger can be configured in the menu.  

Lid open mode may also be manually triggered by pressing the Left button from the Home Status Screen at any time the pit temperature is below the set point.  (This is because Lid Open mode ends once the pit temperature returns to the set point).  Lid Open mode may also be canceled by pressing the Left button again during the countdown, but it is generally not necessary to manually exit this mode unless it was entered accidentally.

Low Fan Speed (below 10%)
When operating in automatic mode at fan speeds below 10%, the fan will run in a partial duty cycle.  The fan will run one second out of every ten for each percent required.  This special low speed mode extends the life of the fan and reduces noise.

== Source Modification and Configuration == 
Most configuration is found in hmcore.h.  There defines used to control the inclusion of some features.  To disable them insert // before the item you'd like to disable.  This "comments out" the define and prevents it from being processed.
HEATERMETER_NETWORKING - Enable the WiFi and web server code.  The code is designed to use AsyncLabs's WiShield 1.0/2.0 or YellowJacket 1.0.  If your WiFi shield does not have a dataflash chip on it, make sure you disable both the DFLASH_* defines.
HEATERMETER_SERIAL - Enable per-period temperature updates to be sent out the serial port as well as configuration changes via serial.  The serial configuration protocol is handlde using the same URLs as ther web server, sent via serial, terminated with CR/CRLF/LF.
DFLASH_LOGGING - Enable saving of per-period temperatures to the dataflash chip present on the WiShield.  Requires HEATERMETER_NETWORKING.
DFLASH_SERVING - Enable serving web pages from the dataflash chip present on the WiShield.  Requires HEATERMETER_NETWORKING.
USE_EXTERNAL_VREF - If enabled, use the Vref pin voltage as the reference when doing ADC measurments instead of the internal 5V reference.

Some configuration is via defines and constants, here are some commonly used values:
grillpid.cpp/Rknown - The value of the biasing resistor used in the temperature probes.
hmcore.h/STEINHART - The Steinhart-Hart coefficients used in the probe calculations for pit/food1/food2 (first item) and ambient (second item).
hmcore.h/CSV_DELIMITER - The delimiter used in the CSV data sent by the /csv URL and serial updastes.

== Supported URLS ==
Both Serial and Web
/set?sp=A - Set the setpoint to integer A
/set?pidA=B - Tune PID parameter A to value float B.  A can be b (bias), p (proportional), i (integral), or d (derivitive)
/set?pnA=B - Set probe name A to string B.  B does not support URL encoding at this time.  Probe numbers are 0=pit 1=food1 2=food2 3=ambient
/set?poA=B - Set probe offset A to integer B.  Probe numbers are 0=pit 1=food1 2=food2 3=ambient

Web-only URLs
/ - The index status page.  Some other supporting files are also used by this URL that are not included in this document.
/json - JSON status object.
/csv - CSV-formated status line

== CSV Format ==
SetPoint,Pit,PitMovAvg,Food1,FoodMovAvg,Food2,Food2MovAvg,Ambient,AmbientMovAvg,Fan,FanMovAvg,LidOpenCountdown
