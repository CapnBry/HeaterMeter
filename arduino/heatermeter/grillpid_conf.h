// HeaterMeter Copyright 2013 Bryan Mayland <bmayland@capnbry.net>

#define GRILLPID_CALC_TEMP
#define GRILLPID_SERIAL_ENABLED
#define GRILLPID_SERVO_ENABLED
#define GRILLPID_DYNAMIC_RANGE
//#define GRILLPID_FEEDVOLT_DEBUG

#define TEMP_PIT    0
#define TEMP_FOOD1  1
#define TEMP_FOOD2  2
#define TEMP_AMB    3
#define TEMP_COUNT  4

// Use oversample/decimation to increase ADC resolution to 2^(10+n) bits n=[0..6]
#define TEMP_OVERSAMPLE_BITS 3

// The time (ms) of the measurement period
#define TEMP_MEASURE_PERIOD 1000
// Number of times the ouput is adusted over TEMP_MEASURE_PERIOD
// This affects fan boost mode and FFEEDBACK output
#define TEMP_OUTADJUST_CNT 3
// 2/(1+Number of samples used in the exponential moving average)
#define TEMPPROBE_AVG_SMOOTH (2.0f/(1.0f+60.0f))
#define PIDOUTPUT_AVG_SMOOTH (2.0f/(1.0f+240.0f))
// Once entering LID OPEN mode, the minimum number of seconds to stay in
// LID OPEN mode before autoresuming due to temperature returning to setpoint
#define LIDOPEN_MIN_AUTORESUME 30

// Servo refresh period in usec, 20000 usec = 20ms = 50Hz
#define SERVO_REFRESH          20000

#if HM_BOARD_REV == 'A'
  #undef GRILLPID_SERVO_ENABLED
#endif
