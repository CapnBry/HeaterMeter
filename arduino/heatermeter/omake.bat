@ECHO OFF
IF NOT DEFINED ARDUINO_PATH SET ARDUINO_PATH=C:\Arduino\arduino-1.0

%ARDUINO_PATH%\hardware\tools\avr\utils\bin\make %*