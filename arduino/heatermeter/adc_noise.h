//#define HEATERMETER_NOISE_ADC_PIN 0

#if defined(HEATERMETER_NOISE_ADC_PIN)
static volatile bool _adcBusy;
ISR(ADC_vect) { _adcBusy = false; }
void testNoise(void)
{
  _adcBusy = false;
  ADMUX = (DEFAULT << 6) | HEATERMETER_NOISE_ADC_PIN;
  ADCSRA |= bit(ADATE) | bit(ADIE);
  ADCSRB &= ~(bit(ADTS2) | bit(ADTS1) | bit(ADTS0));
  ADCSRA |= bit(ADSC);
  while (_adcBusy) { };
  _adcBusy = true;

  uint16_t high = 0;
  uint16_t low = 1024;
  uint32_t avg = 0;
  uint8_t arr[16];
  memset(arr, 0, sizeof(arr));
  for (uint8_t i=0; i<255; ++i)
  {
    while (_adcBusy) { };
    _adcBusy = true;
    uint16_t adc = ADC;
    avg += adc;
    if (adc < 64)
      ++arr[adc/4];
    else
      ++arr[15];
    if (adc > high) high = adc;
    if (adc < low) low = adc;
  }
  ADCSRA &= ~(bit(ADATE) | bit(ADIE));

  SerialX.print("HMLG,Noise lah=");
  SerialX.print(low, DEC);
  SerialX.print(' ');
  SerialX.print(avg / 255.0f, 1);
  SerialX.print(' ');
  SerialX.print(high, DEC);
  SerialX.print(' ');
  for (uint8_t j=0; j<16; ++j)
  {
    SerialX.print(' ');
    SerialX.print(arr[j], DEC);
  }
  Serial_nl();

  pid.Probes[3]->Temperature = high;
}
#else
void testNoise(void) { }
#endif
