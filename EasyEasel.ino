int PWM_PIN = 5;
int LEFT_BUTTON_CLAMP = 7; // marker clamp
int RIGHT_BUTTON_RELEASE = 8; // marker release but also soap dispense
float SOAP_DISPENSE_POWER = -0.07;
float MARKER_CLAMP_POWER = 1.0;
bool clampButtonState = false;
bool releaseButtonState = false;

void setup()
{
  // Serial.begin(9600);
  pinMode(PWM_PIN, OUTPUT);
  pinMode(LEFT_BUTTON_CLAMP, INPUT);
  pinMode(RIGHT_BUTTON_RELEASE, INPUT);
}

void loop()
{
  clampButtonState = digitalRead(LEFT_BUTTON_CLAMP);
  releaseButtonState = digitalRead(RIGHT_BUTTON_RELEASE);

  if (clampButtonState == HIGH){
    analogWrite(PWM_PIN, dutyCycleToWaveForm(MARKER_CLAMP_POWER));
  } else if (releaseButtonState == HIGH){
    analogWrite(PWM_PIN, dutyCycleToWaveForm(SOAP_DISPENSE_POWER));
  }
  
}

int dutyCycleToWaveForm(float dutyCycle){
  return (int) (dutyCycle * 255);
}

