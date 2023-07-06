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
int MARKER_CLAMP_BUTTON = 8;
int SOAP_DISPENSE_BUTTON = 6;
int RESET_BUTTON = 7;
int PWM_PIN = 9;

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

Servo myservo;

void setup() {
  Serial.begin(9600);
  myservo.attach(PWM_PIN);
  delay(1000); // pwm duty cycle is scuffed immediately after setup 
}

void loop() {
  int applied_output = -1337; // sentinel applied output (not possible !!)
  easel_state next_state = current_state;
  switch(current_state){
    case UNINITIALIZED:
      // Output

      // Transition
      next_state = HOMING;
      break;
    case HOMING:
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

      // Transition
      if (isHomed){
        startedHoming = false; // resetting the homing variables for the next time we need to home
        isHomed = false;

        myservo.write(0); // quick finesse ong

        next_state = RESET_TO_IDLE;
      }
      break;
    case RESET_TO_IDLE:
      // Output
      // We assume that we're at homed position when we're in this state

      if (!startedResetToIdle){
        lastResetToIdle = millis();
        startedResetToIdle = true;
      }

      if (millis() - lastResetToIdle <= secondsToMilliseconds(0.65)){
        applied_output = RESET_TO_IDLE_POWER;
      } else {
        isAtIdle = true;
      }

      // Transition
      if (isAtIdle){
        startedResetToIdle = false;

        next_state = IDLE;
      }
      break;
    case IDLE:
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

      break;
    case MARKER_CLAMP:
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
        next_state = HOMING;
      }
      break;
    case SOAP_DISPENSE:
      // Output
      if (!startedDispensing){
        lastDispenseTime = millis();
        startedDispensing = true;
      }

      // Transition
      if (millis() - lastDispenseTime <= secondsToMilliseconds(0.75)){
        applied_output = SOAP_DISPENSE_POWER;
      } else {
        startedClamping = false;
        next_state = HOMING;
      }
      break;
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

  Serial.println("Current State");
  Serial.println(current_state);
}

int secondsToMilliseconds(float seconds){
  return seconds * 1000;
}