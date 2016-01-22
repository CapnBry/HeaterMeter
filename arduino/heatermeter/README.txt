HeaterMeter Instructions

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
Lid Open Offset - While temperature is being automatically maintained, if the temperature drops this percent from the Set Point, HeaterMeter will automatically enter Lid Open mode.  For example, if the Lid Open Offset is 6% and the Set Point is 250F, Lid Open mode will automatically be triggered below 235F.
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
HEATERMETER_SERIAL (baud)- Enable per-period temperature updates to be sent out the serial port as well as configuration changes via serial.  The serial configuration protocol is handlde using the same URLs as ther web server, sent via serial, terminated with CR/CRLF/LF.
HEATERMETER_RFM12 (band) - Enable the RFM12 device server.
DFLASH_SERVING - Enable serving web pages from the dataflash chip present on the WiShield.  Requires HEATERMETER_NETWORKING.
USE_EXTERNAL_VREF - If enabled, use the Vref pin voltage as the reference when doing ADC measurments instead of the internal 5V reference.
PIEZO_HZ (hertz) - Peak output frequency of the piezo alarm attached to the system. If not defined, build without sound support.
SHIFTREGLCD_NATIVE - If defined, use original ShiftRegLCD code instead of SPIShiftRegLCD. "Native" mode is needed for HeaterMeter PCB version 3.1 and below.

Some configuration is via defines and constants, here are some commonly used values:
strings.h/CSV_DELIMITER - The delimiter used in the CSV data sent by the /csv URL and serial updastes.

WiShield Wireless parameters are stored in wishieldconf.h.

== Supported URLS ==
Note: No url can exceed a maximum length of 63 bytes

Both Serial and Web
/set?sp=AU - Set the setpoint to integer A with optional units U. Supported Units are (A)DC Raw, (F)ahrenheit , (C)elcius, and (R)esistance. Setting the setpoint to a negative value switches to "manual mode" where the output percentage is set directly (-0 for 0%).
/set?pidA=B - Tune PID parameter A to value float B.  A can be b (bias), p (proportional), i (integral), or d (derivative)
/set?pnA=B - Set probe name A to string B.  B does not support URL encoding at this time.  Probe numbers are 0=pit 1=food1 2=food2 3=ambient
/set?po=A,B,C,D - Set probe offsets to integers A, B, C, and D. Offsets can be omitted to retain their current values, such as po=,,,-2 to only set probe number 3's offset to -2
/set?pcN=A,B,C,R,TRM - Set the probe coefficients and type for probe N.  A, B, and C are the Steinhart-Hart coeffieicents and R is the fixed side of the probe voltage divider.  A, B, C and R are floating point and can be specified in scienfific noation, e.g. 0.00023067434 -> 2.3067434e-4.  TRM is either the type of probe OR an RF map specifier.  If TRM is less than 128, it indicates a probe type.  Probe types are 0=Disabled, 1=Internal, 2=RFM12B.  Probe types of 128 and above are implicitly of type RFM12B and indicate the transmitter ID of the remote node (0-63) + 128. e.g. Transmitter ID 2 would be passed as 130. The value of 255 (transmitter ID 127) means "any" transmitter and can be used if only one transmitter is used.  Any of A,B,C,R,TRM set to blank will not be modified. Probe numbers are 0=pit 1=food1 2=food2 3=ambient
/set?lb=A,B,C[,C...] - Set display parameters.  A = LCD backlight Range is 0 (off) to 255 (full). B = Home screen mode 254=4-line 255=2-line 0, 1, 2, 3 = BigNum. C = Set LED config byte for Nth LED. See ledmanager.h::LedStimulus for values. High bit means invert.
/set?ld=A,B,C - Set Lid Detect offset to A%, duration to B seconds. C is used to enable or disable a currently running lid detect mode. Non-zero will enter lid open mode, zero will disable lid open mode.
/set?al=L,H[,L,H...] - Set probe alarms thresholds. Setting to a negative number will disable the alarm, setting to 0 will stop a ringing alarm and disarm it.
/set?fn=FL,FH,SL,SH,Flags,MSS,FAF,SAC - Set the fan output parameters. FL = min fan speed before "long PID" mode, FH = max fan speed, SL = Servo Low (in 10x usec), SH = Servo High (in 10x usec), MSS = Max Startup Speed, FAF = Fan active floor, SAC = Servo active ceiling. Flags = Bitfield 0=Invert Fan, 1=Invert Servo
/set?tt=XXX[,YYY] - Display a "toast" message on the LCD which is temporarily displayed over any other menu and is cleared either by timeout or any button press. XXX and YYY are the two lines to displau and can be up to 16 characters each.
/set?tp=A - Set a "temp param". A = Log PID Internals ($HMPS)
/reboot - Reboots the microcontroller.  Only if wired to do so (LinkMeter)

Serial-only URLs
/set?pnXXX - Retrieve the current probe names
/config - Retreives version, current probe names and RF map (will be expanded to all config at some point)

Web-only URLs
/ - The index status page.  Some other supporting files are also used by this URL that are not included in this document.
/json - JSON status object.

== CSV Format ==
Format is similar to the NMEA 0183 format, starting with a dollar sign ($), talker ID (2 characters) and message ID (2 characters) and ends with an asterisk (*) and checksum. The checksum is the two digit hexadecimal representation of an XOR of all the characters between the $ and *. The sentence is ended with linefeed only (ASCII 10).
Example: $HMXX,,,,,,*DB\n
Microcontroller ID
$UCID,HeaterMeter,VersionID
Alarm Indicator
$HMAL,LowProbe0,HighProbe0[,...] (L or H suffix indicates ringing, negative values indicated disabled alarms)
Fan Parameters
$HMFN,Low,High,ServoMin,ServoMax,Flags,MaxStartup,FanActiveFloor,ServoActiveCeil
Display Parameters
$HMLB,LCDBacklight,LCDHomeMode,LED0,LED1,LED2,LED3
Lid Detect Parameters
$HMLD,Offset Percent,Lid Duration
Debug Log Message
$HMLG,Message
PID Coefficients
$HMPD,PidB,PidP,PidI,PidD 
Probe Names
$HMPN,Probe0,Probe1,Probe2,Probe3
Probe Offsets
$HMPO,Probe0,Probe1,Probe2,Probe3
PID Internal Status (Sum cPID* to get output)
$HMPS,cPidB,cPidP,cPidI,cPidD,tempD
PID State Update
$HMSU,SetPoint,Pit,Food1,Food2,Ambient,Fan,FanMovAvg,LidOpenCountdown
RF Status
$HMRF,255,0,CrcStatus[,NodeId,Flags,Rssi...]
RF Mapping
$HMRM,SourceId,SourceId,SourceId,SourceId
