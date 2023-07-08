#include <Servo.h>

enum easel_state{
  UNINITIALIZED,
  HOMING,
  MARKER_CLAMP,
  SOAP_DISPENSE,
  RESET_TO_IDLE,
  IDLE
};

enum easel_request{
  MARKER_CLAMP_REQ,
  SOAP_DISPENSE_REQ,
  RESET_REQ,
  IDLE_REQ,
};

int SOAP_DISPENSE_POWER = 87;
int MARKER_CLAMP_POWER = 100;
int HOME_POWER = 120;
int RESET_TO_IDLE_POWER = 88;

float MAX_POS = 8.37862759755; // inches
float GEAR_DIAMETER = 2.667; // inches
float OPI = 3.1415926535;

int SOAP_DISPENSE_BUTTON = 6;
int MARKER_CLAMP_BUTTON = 8;
int RESET_BUTTON = 7;
int PWM_PIN = 9;
int ENCODER_PIN = 3;

easel_state current_state = UNINITIALIZED;
easel_request current_request = IDLE_REQ;

bool startedHoming = false;
bool isHomed = false;
unsigned long lastHomeTime;

bool startedResetToIdle = false;
bool isAtIdle = false;
unsigned long lastResetToIdle;

bool startedClamping = false;
unsigned long lastClampTime;

bool startedDispensing = false;
unsigned long lastDispenseTime;

unsigned long highPulse;
unsigned long lowPulse;

float pos = 0.0;
float lastPos = 0.0;
float offset = 0.0;

Servo myservo;

void setup() {
  Serial.begin(9600);
  myservo.attach(PWM_PIN);
  delay(1000); // pwm duty cycle is scuffed immediately after setup 
}

void loop() {
  int applied_output = -1337; // sentinel applied output (not possible !!)
  float encoderPosition = getEncoderPosition();
  easel_state next_state = current_state;
  if(current_state == UNINITIALIZED){
      // Output

      // Transition
      next_state = HOMING;
  } else if(current_state == HOMING){
    // Output
    if (!startedHoming){
      lastHomeTime = millis(); // only want to set the home time once (state machines are periodic)
      startedHoming = true;
    }

    if (millis() - lastHomeTime <= secondsToMilliseconds(0.4)){
      applied_output = HOME_POWER;
    } else {
      isHomed = true; // been homing for more than 5 seconds, assume we're at 0 position
    }

    // setting positions to 0
    pos = 0.0;
    lastPos = encoderPosition;

    // Transition
    if (isHomed){
      startedHoming = false; // resetting the homing variables for the next time we need to home
      isHomed = false;


      next_state = RESET_TO_IDLE;
    }
  } else if(current_state == IDLE){
      // Output
      // Do nothing during Idle

      // Transition
      switch (current_request){
        case IDLE_REQ:
          next_state = IDLE;
          break;
        case MARKER_CLAMP_REQ:
          next_state = MARKER_CLAMP;
          break;
        case SOAP_DISPENSE_REQ:
          next_state = SOAP_DISPENSE;
          break;
        case RESET_REQ:
          next_state = HOMING;
          break;
      }

      if (next_state != IDLE){
        isAtIdle = false;
      }
    } else if(current_state == MARKER_CLAMP){
      // Output

      if (!startedClamping){
        lastClampTime = millis();
        startedClamping = true;
      }

      if (millis() - lastClampTime <= secondsToMilliseconds(5)){
        applied_output = MARKER_CLAMP_POWER; 
      }

      // Transition
      if (current_request == RESET_REQ){
        startedClamping = false;
        next_state = IDLE;
      }
    } else if(current_state == SOAP_DISPENSE){
      // // Output
      // if (!startedDispensing){
      //   lastDispenseTime = millis();
      //   startedDispensing = true;
      // }

      // // Transition
      // if (millis() - lastDispenseTime <= secondsToMilliseconds(0.75)){
      //   applied_output = SOAP_DISPENSE_POWER;
      // } else {
      //   startedClamping = false;
      //   next_state = HOMING;

      // We assume that we're at homed position when we're in this state

      bool reachedPosition = false;

      float desiredPosition = 4.0;

      if ((abs(pos - desiredPosition) <= 0.5)){
        applied_output = 92;
        reachedPosition = true;
      } else if ((pos - desiredPosition) < 0){
        applied_output = 88; // outward
      } else {
        applied_output = 96; // inward
      }

      // Transition
      if (reachedPosition){
        next_state = IDLE;
      }
    } else if(current_state == RESET_TO_IDLE){
      // Output
      // We assume that we're at homed position when we're in this state

      bool reachedPosition = false;

      float desiredPosition = 1.13;

      if ((abs(pos - desiredPosition) <= 0.5)){
        applied_output = 92;
        reachedPosition = true;
      } else if ((pos - desiredPosition) < 0){
        applied_output = 88; // outward
      } else {
        applied_output = 96; // inward
      }

      // Transition
      if (reachedPosition){
        next_state = IDLE;
      }
  }

  current_state = next_state;

  if (applied_output != -1337){
    myservo.write(applied_output);
  } else {
    myservo.write(92);
  }

  if (digitalRead(MARKER_CLAMP_BUTTON) == HIGH){
    current_request = MARKER_CLAMP_REQ;
  } else if (digitalRead(SOAP_DISPENSE_BUTTON) == HIGH){
    current_request = SOAP_DISPENSE_REQ;
  } else if (digitalRead(RESET_BUTTON) == HIGH){
    current_request = RESET_REQ;
  } else {
    current_request = IDLE_REQ;
  }

  // we want continuity going from 1.0 on encoder to 0.0
  float delta1 = (encoderPosition - lastPos); 
  float delta2 = (encoderPosition - lastPos + MAX_POS);
  float delta3 = (encoderPosition - lastPos - MAX_POS);

  // checking to see which of these deltas is the smallest in abs value helps us do that
  float pos_delta = minimumInAbsoluteValue(delta1, delta2, delta3);

  
  if (!isnan(encoderPosition)){
    if (encoderPosition < 0.1 && lastPos > 8.3){
      pos += ((MAX_POS - lastPos) + encoderPosition);
    } else if (encoderPosition > 8.3 && lastPos < 0.1){
      pos += ((MAX_POS - encoderPosition) - lastPos);
    } else {
      pos += encoderPosition - lastPos;
    }

    lastPos = encoderPosition;
  }

  Serial.println(pos);
  
}

int secondsToMilliseconds(float seconds){
  return seconds * 1000;
}

float minimumInAbsoluteValue(float val1, float val2, float val3){
  float magnitude = min(abs(val1), min(abs(val2), abs(val3)));

  if (magnitude == abs(val1)){
    return val1;
  }

  if (magnitude == abs(val2)){
    return val2;
  }

  if (magnitude == abs(val3)){
    return val3;
  }
}

float getOffsetEncoderPosition(){
  return pos;
}

float getEncoderPosition(){

  // values aren't atomic so we need to keep reading until we get 2 same readings
  for (int i = 0; i < 10; i++){
    float pos1 = convertedEncoderOutput();
    float pos2 = convertedEncoderOutput();

    if (epsilonEquals(pos1, pos2)){
      return pos1;
    }
  }
}

float convertedEncoderOutput(){
  highPulse = pulseIn(ENCODER_PIN, HIGH);
  lowPulse = pulseIn(ENCODER_PIN, LOW);

  float dutyCycle = (float) highPulse / (float) (highPulse + lowPulse);

  float encoderPosition = dutyCycle * (GEAR_DIAMETER * OPI);

  return encoderPosition;
}

bool epsilonEquals(float a, float b){
  return abs(a - b) < 0.1;
}