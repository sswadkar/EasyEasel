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
int MARKER_CLAMP_POWER = 160;
int HOME_POWER = 120;
int RESET_TO_IDLE_POWER = 88;
int RESET_THRESHOLD_FOR_SOAP_DISPENSE = 2.0; //seconds

float MAX_POS = 8.37862759755; // inches
float GEAR_DIAMETER = 2.667; // inches
float OPI = 3.1415926535;

int SOAP_DISPENSE_BUTTON = 6;
int MARKER_CLAMP_BUTTON = 8;
int RESET_BUTTON = 7;
int PWM_PIN = 9;
int R_PIN = 11;
int G_PIN = 10;
int B_PIN = 12; // b //
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
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  delay(1000); // pwm duty cycle is scuffed immediately after setup 
}

void loop() {
  int applied_output = -1337; // sentinel applied output (not possible !!)
  float encoderPosition = getEncoderPosition();
  easel_state next_state = current_state;
  if(current_state == UNINITIALIZED){
      // Output
      setColor(0, 255, 255);

      // Transition
      next_state = HOMING;
  } else if(current_state == HOMING){
    // Output
    setColor(0, 255, 255);

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
    lastPos = encoderPosition; // delta should read 0

    // Transition
    if (isHomed){
      startedHoming = false; // resetting the homing variables for the next time we need to home
      isHomed = false;


      next_state = RESET_TO_IDLE;
    }
  } else if(current_state == IDLE){
      // Output
      setColor(255, 0, 255);
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
      setColor(0,0,255);

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
      // Output
      setColor(255,255,0);

      if (!startedDispensing){
        lastDispenseTime = millis();
        startedDispensing = true;
      }

      bool reachedPosition = false;

      float desiredPosition = 2.75;

      if ((abs(pos - desiredPosition) <= 0.5)){
        applied_output = 92;
        reachedPosition = true;
      } else if ((pos - desiredPosition) < 0){
        applied_output = 85; // outward
      } else {
        applied_output = 96; // inward
      }

      // Transition
      if (reachedPosition || (millis() - lastDispenseTime <= secondsToMilliseconds(RESET_THRESHOLD_FOR_SOAP_DISPENSE))){
        startedDispensing = false;
        next_state = RESET_TO_IDLE;
      }
    } else if(current_state == RESET_TO_IDLE){
      // Output
      setColor(255, 0, 255);
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
    delay(2000);
  } else if (digitalRead(SOAP_DISPENSE_BUTTON) == HIGH){
    current_request = SOAP_DISPENSE_REQ;
    delay(2000);
  } else if (digitalRead(RESET_BUTTON) == HIGH){
    current_request = RESET_REQ;
  } else {
    current_request = IDLE_REQ;
  }

  
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

  Serial.println(current_state);
  
}

int secondsToMilliseconds(float seconds){
  return seconds * 1000;
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

void setColor(int r, int g, int b){
  analogWrite(R_PIN, r);
  analogWrite(G_PIN, g);
  analogWrite(B_PIN, b);
}