// HeaterMeter Copyright 2018 Bryan Mayland <bmayland@capnbry.net>
/*
 * Compensate for non-linearity of thermocouple seebeck voltage.
 * Data cones from the "Actual AD849x Results" table [1] and not the NIST 
 * standard [2] thermocouple voltages table, which seems to produce values
 * <0.3C lower when used with the function Vout = NIST(temp) * 122.4 - 1.25mV
 * [1] https://srdata.nist.gov/its90/type_k/0to300.html
 * [2] http://www.analog.com/media/en/technical-documentation/application-notes/AN-1087.PDF

 * Algorithm is just a simple LERP from table values between 0C and 340C
 * although the values from 320C-340C are off by as much as 0.5mV (0.1C)
 * at 340C
 *
 * Who puts actual code in a header file? Ech. I just didn't want to make
 * it part of the TempProbe class.
*/

static const int8_t NONLIN[] PROGMEM = {
  /* Based on Table 3 from AN-1087, table is 0.01C offsets  */
  -63,  // 0C ideal=0.000V act=0.003V -62.5
  0,    // 20C = 0 offset
  0,    // 40C = 0 offset
  -20,  // 60C ideal=0.300V act=0.301V -19.8
  -40,  // 80C ideal=0.400V act=0.402V -39.6
  -78,  // 100C ideal=0.500V act=0.504V -78.4
  -99,  // 120C ideal=0.600V act=0.605V -99.0
  -100, // 140C ideal=0.700V act=0.705V -100.0
  -61,  // 160C ideal=0.800V act=0.803V -61.2
  -20,  // 180C ideal=0.900V act=0.901V -20.4
  20,   // 200C ideal=1.000V act=0.999V 20.4
  61,   // 220C ideal=1.100V act=1.097V 61.2
  81,   // 240C ideal=1.200V act=1.196V 80.8
  101,  // 260C ideal=1.300V act=1.295V 101.0
  79,   // 280C ideal=1.400V act=1.396V 79.2
  59,   // 300C ideal=1.500V act=1.497V 59.4
  20,   // 320C ideal=1.600V act=1.599V 19.6
        // 330C = 0 offset, after that the error grows by about 0.035C/C
};

static const int8_t NONLIN_STEP = 20;
static const int8_t NONLIN_CNT = sizeof(NONLIN) / sizeof(NONLIN[0]);

static float tcNonlinearCompensate(float tempC)
{
  /* compensate for non-linearity of thermocouples using a lerp between compensated values */
  if ((tempC <= 0) || (tempC >= (NONLIN_STEP * NONLIN_CNT)))
    return tempC;

  int8_t idx = (int16_t)tempC / NONLIN_STEP;
  int8_t nonlin_1 = pgm_read_byte(&NONLIN[idx]);
  int8_t nonlin_2 = (idx < NONLIN_CNT - 1) ? pgm_read_byte(&NONLIN[idx + 1]) - nonlin_1 : -nonlin_1;
  float offset = (tempC - (NONLIN_STEP * idx)) / (float)NONLIN_STEP * nonlin_2 + nonlin_1;

  return tempC + (offset / 100.0f);
}

