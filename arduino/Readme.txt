PID Flag Mode 5
----------
Concept is that the fan is a booster with 10 different speeds
When servo is above(70%) or below(30%) a "shift" point then the fan will react according.
After a shift it will move the the servo(5%) back towards the center as an attempt to give the PID controls an easier chance to react to the change in airflow.
Fan will be blocked from shifting again for 30 seconds in order to give PID time to react.

In manual mode there should be no change. On lid opening the fan may take up to 30 seconds to shutoff but will go immediately to off if Servo is at 0%.

Compile time defines currently in Grillpid_conf.h
-----------------------------
Upshift point is currently defined at 70%
Downshift point is current defined at 30%
Each fan shift is 10% of max fan speed
Hold time between shifts is 30 seconds


Example 1
---------
Servo has opened up to 70% and fan was off. Fan will start at 10% of max( either startup or steady). Servo will get bumped back to 65%.
30 seconds later the servo is once again over 70%. Fan will now go to 20% of max. Servo will get closed down 5% once again.
This cycle will repeat until either the fan is at 100% of max or the PID moves the servo less than 70% but above 30%

Example 2
---------
Servo is running about 50% open and the fan is at 20%. User notices the top vent is closed too much and opens it open.
PID starts running the servo closed as the extra airflow is causing a heatup.
As the servo goes under 30% the fan drops to 10% and servo gets opened up 5%.
30 seconds later the servo is back under 30%. Fan will now shutoff and once again add 5%.
Several minutes the pit will stabilize with the servo somewhere under 70% ( likely under 30% )

