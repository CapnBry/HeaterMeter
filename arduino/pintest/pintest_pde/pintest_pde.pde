const unsigned char OUT = 4;
const unsigned char IN = 5;

void setup()
{
  Serial.begin(9600);
  pinMode(OUT, OUTPUT);
  digitalWrite(OUT, LOW);
  pinMode(IN, INPUT);
}

void loop()
{
  delay(1000);
  
  digitalWrite(OUT, HIGH);
  if (digitalRead(IN))
    Serial.print("HIGH: OK, ");
  else
    Serial.print("HIGH: FAIL, ");

  digitalWrite(OUT, LOW);
  if (digitalRead(IN))
    Serial.println("LOW: FAIL");
  else
    Serial.println("LOW: OK");
}
