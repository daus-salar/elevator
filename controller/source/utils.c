#define __AVR_ATmega64__
#include <Arduino.h>


#ifdef DEBUG_WIRE
#include <Wire.h>
#endif

// Generic constants
#define LEVEL_NUMBER 3 // Amount of the levels, corresponds with levels 0, 1 and 2.

// Door related constants
#define DOOR_SPEED            2000  // Milliseconds for full open or close movement.
#define DOOR_CLOSED_POSITION     0  // Position value for a closed door.
#define DOOR_OPEN_POSITION     100  // Position value for an open door.
#define DOOR_WAIT_TIME        4000  // Milliseconds the doors will wait in the open position.
#define DOOR_LEFT_OPEN          50  // TODO: Check this value!
#define DOOR_LEFT_CLOSED       200  // TODO: Check this value!
#define DOOR_RIGHT_OPEN        200  // TODO: Check this value!
#define DOOR_RIGHT_CLOSED       50  // TODO: Check this value!

// Motor related constants
#define FAST_MOTOR_SPEED       255
#define SLOW_MOTOR_SPEED       128

// register interface constants
#define I2C_BUFFER 5                // 1 address byte and 4 byte to set

// Pin constants
#define PIN_O_MOTOR_ENABLE       2  // port pin 3
#define PIN_O_MOTOR_UP           3  // port pin 4
#define PIN_I_SAFETY_UP         52  // port pin 5
#define PIN_O_MOTOR_DOWN         4  // port pin 6
#define PIN_I_SAFETY_DOWN       50  // port pin 7
#define PIN_I_TEMPERATURE       11  // port pin 10
#define PIN_I_ENCODER_A         -1  // port pin 11
#define PIN_I_ENCODER_B         -1  // port pin 12
#define PIN_I_LEVEL_1           -1  // port pin 13
#define PIN_I_LEVEL_2           -1  // port pin 14
#define PIN_I_LEVEL_3           -1  // port pin 15
#define PIN_O_DOOR_LEFT          5  // port pin 16
#define PIN_O_DOOR_RIGHT         6  // port pin 17
#define PIN_I_LEVEL_BUTTON_1    -1  // port pin 18
#define PIN_O_LEVEL_LIGHT_1      7  // port pin 19
#define PIN_I_LEVEL_BUTTON_2    -1  // port pin 20
#define PIN_O_LEVEL_LIGHT_2      8  // port pin 21
#define PIN_I_LEVEL_BUTTON_3    -1  // port pin 22
#define PIN_O_LEVEL_LIGHT_3      9  // port pin 23
#define PIN_O_CABIN_LIGHT       10  // port pin 24

// ------------------------------
// Enum definitions
// ------------------------------

// State of the elivator
enum OperationState {
  init_state, sleep_state, move_state, opendoors_state, wait_state, closedoors_state, maintenance_state, testc_state
};

enum PositionState {
  unknown, far_below, close_below, reached, close_above, far_above
};

enum MotorSpeed {
  stopped, slow, fast
};

enum MotorDirection {
  up, down
};

enum LightMode {
  off, on, flashing
};

enum DoorState {
  open, closed, opening, closing
};

// Funktonsprototype
void stopCabinMotor(void);
void testc(void);
void setState(enum OperationState newState);
void setOutsideLevelStates(int level);
void transferInputs(void);
int findTargetLevel(void);
void moveCabin(void);
void openDoors(void);
void closeDoors(void);
void moveDoors(int fromPosition, int toPosition);
void wait(void);
void maintenance(void);
void initialize(void);
void sleep(void);

//Description
//Re-maps a number from one range to another. That is, a value of fromLow would get mapped to toLow, 
//a value of fromHigh to toHigh, values in-between to values in-between, etc. 
//Parameters
//
//value: the number to map
//fromLow: the lower bound of the value's current range
//fromHigh: the upper bound of the value's current range
//toLow: the lower bound of the value's target range
//toHigh: the upper bound of the value's target range
//Returns
//The mapped value.
// Code:
//   return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
extern long map(long x, long in_min, long in_max, long out_min, long out_max);

// ------------------------------
// Global variables
// ------------------------------

// State handling
enum OperationState state = init_state; // Current state of the state machine
enum OperationState last_state = init_state; // Previous state of the state machine in the last cycle
long state_time = 0;
long state_cycle = 0; // Holds the number of times the current state is iterated.

// Position handling
int level_position = -1;
int level_target = -1;
enum PositionState level_position_state[LEVEL_NUMBER];
int last_blocked_level = -1;
boolean button_state[LEVEL_NUMBER];
boolean safetyUp = false; // State of the safety switch above the highest floor
boolean safetyDown = false; // State of the safety switch below the lowest floor

// Temperature variable
float motorTemperature = -1;

// Global variables for door movement
long door_start_time = 0; // Moment of door movement begin.
int door_position = 0; // Holds door position as value between DOOR_CLOSED_POSITION and DOOR_OPEN_POSITION.

// Global variables for encoder interpretion
int encoder_value = 0;
long encoder_time = 0;
long encoder_ticks = 0;
float encoder_speed = 0;
boolean encoder_overspeed = false;

// Global variable for motor control
enum MotorDirection motor_direction;
enum MotorSpeed motor_speed;

// Global variable for register interface
int nbr_of_received_bytes = 0;
int received_values[I2C_BUFFER];
int request_queue[100];
int queue_start = 0;
int queue_end = 0;

void transferButtonInputs();
void transferEncoderInput();
void transferLevelSensors();
void setCabinLight(enum LightMode light);
void setButtonLight(int level, enum LightMode light);
void moveCabinMotor(enum MotorSpeed speed, enum MotorDirection direction);


// ------------------------------

// Possible values are 0, 1, 2 and 3. To interpret this value handle the two bits as separate values.
int readEncoderValue() {
  int a = digitalRead(PIN_I_ENCODER_A);
  int b = digitalRead(PIN_I_ENCODER_B);
  return a + (b << 2);
}

float readTemperature() {
  int i;
  float temp              = 82;
  ADCSRA = 0x00;
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
  ADMUX = 0x00;
  ADMUX = (1 << REFS0);
  ADMUX |= PIN_I_TEMPERATURE;

  for (i = 0; i <= 64; i++)
  {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    temp += (ADCL + ADCH * 256);
  }

  temp /= 101;
  temp -= 156;
  return (temp);
}

// ------------------------------

// Arduino Setup Function. Will be called once after system boot.
void setup() {
  int i;
  // put your setup code here, to run once:
  // Setup arrays
  for (i = 0; i < LEVEL_NUMBER; i++) {
    button_state[i] = false;
    level_position_state[i] = unknown;
  }

  // Setup input and output channels
  pinMode(PIN_O_MOTOR_ENABLE, OUTPUT);
  pinMode(PIN_O_MOTOR_UP, OUTPUT);
  pinMode(PIN_O_MOTOR_DOWN, OUTPUT);
  pinMode(PIN_O_DOOR_LEFT, OUTPUT);
  pinMode(PIN_O_DOOR_RIGHT, OUTPUT);
  pinMode(PIN_O_LEVEL_LIGHT_1, OUTPUT);
  pinMode(PIN_O_LEVEL_LIGHT_2, OUTPUT);
  pinMode(PIN_O_LEVEL_LIGHT_3, OUTPUT);
  pinMode(PIN_O_CABIN_LIGHT, OUTPUT);

#ifdef DEBUG_WIRE
  // Setup i2c bus
  Wire.begin(4);                // join i2c bus with address #4
  Wire.onReceive(receiveEvent); // register event
  Wire.onRequest(requestEvent); // register event

  // Setup serial
  Serial.begin(9600);           // start serial for output
  Serial.println("setup end");
#endif

  // Init state machine and input variables
  setState(init_state);
}

// Arduino Loop Function. Will be called in an endless loop.
void loop() {
  state_cycle++;

  transferInputs();

  switch (state) {
case init_state:
      initialize();
      break;
case sleep_state:
      sleep();
      break;
case move_state:
      moveCabin();
      break;
case opendoors_state:
      openDoors();
      break;
case wait_state:
      wait();
      break;
case closedoors_state:
      closeDoors();
      break;
case maintenance_state:
      maintenance();
      break;
case testc_state:
      testc();
  }
}

void testc(void) {
  // TODO:
}

void setState(enum OperationState newState) {
  state_time = millis();

  // TODO: accept only valid state changes

  last_state = state;
  state = newState;
  state_cycle = 0; // reset the state cycle
}

void transferTemperature(void) {
  motorTemperature = readTemperature();
}

void transferInputs(void) {
  transferButtonInputs();
  transferEncoderInput();
  transferLevelSensors();
  transferTemperature();
}

void wait() {
  long time = millis(); // The current time in millis

  if (time >= state_time + DOOR_WAIT_TIME) {
    setState(closedoors_state);
  } else {
    delay(1);
  }
}

void maintenance() {
  int i;
  setCabinLight(off);

  for (i = 0; i < LEVEL_NUMBER; i++) {
    setButtonLight(i, flashing);
  }
}

void sleep() {
  int i;
  for (i = 0; i < LEVEL_NUMBER; i++) {
    if (button_state[i] && level_position_state[i] == reached) {
      setState(opendoors_state);
      return;
    }
  }

  for (i = 0; i < LEVEL_NUMBER; i++) {
    if (button_state[i] && level_position_state[i] != reached) {
      setState(move_state);
      return;
    }
  }
}

void initialize() {
  int i;
  closeDoors();
  setCabinLight(on);
  for (i = 0; i < LEVEL_NUMBER; i++) {
    setButtonLight(i, off);
    button_state[i] = false;
  }

  for (i = 0; i < LEVEL_NUMBER; i++) {
    if (level_position_state[i] == reached) {
      setState(sleep_state);
      return;
    }
  }

  // Move Cabin slowly down until level is reached or motor stops because of safety switch.
  // If Cabin at lowest point move slowly up until level is reached.

  moveCabinMotor(slow, down);

  int safety = 0;
  
  // TODO: Stop wenn all level_position_states are known!
}

void intToCharArray(char *buf, int val) {
  buf[0] = (val >> 24) & 0xff;
  buf[1] = (val >> 16) & 0xff;
  buf[2] = (val >> 8) & 0xff;
  buf[3] = val & 0xff;
}

int arrayToInt(int *buf) {
  int val; 
  val = buf[0] << 24;
  val |= buf[1] << 16;
  val |= buf[2] << 8;
  val |= buf[3];
  return val;
}

#ifdef DEBUG_WIRE
// i2c receive event
void receiveEvent(int howMany) {

  nbr_of_received_bytes = 0;
  while (Wire.available() > 0) {
    if (nbr_of_received_bytes < I2C_BUFFER && state == testc_state) {
      received_values[nbr_of_received_bytes++] = Wire.read();
    } else {
      Wire.read();
    }
  }
  if (nbr_of_received_bytes == I2C_BUFFER) {
    int address = received_values[0];

    if (address == 1) {
      state = (OperationState) arrayToInt(&received_values[1]);

    } else if (address == 2) {
      last_state = (OperationState) arrayToInt(&received_values[1]);

    } else if (address == 3) {
      int tmp = arrayToInt(&received_values[1]);
      state_time = *((long*)&tmp);

    } else if (address == 4) {
      int tmp = arrayToInt(&received_values[1]);
      state_cycle = *((long*)&tmp);

    } else if (address == 5) {
      level_position = arrayToInt(&received_values[1]);

    } else if (address == 6) {
      level_target = arrayToInt(&received_values[1]);

    } else if (address == 7) {
      level_position_state[0] = (PositionState) arrayToInt(&received_values[1]);

    } else if (address == 8) {
      level_position_state[1] = (PositionState) arrayToInt(&received_values[1]);

    } else if (address == 9) {
      level_position_state[2] = (PositionState) arrayToInt(&received_values[1]);

    } else if (address == 10) {
      last_blocked_level = arrayToInt(&received_values[1]);

    } else if (address == 11) {
      int boolean_as_int = arrayToInt(&received_values[1]);
      button_state[0] = false;
      if (boolean_as_int == 1) {
        button_state[0] = true;
      }

    } else if (address == 12) {
      int boolean_as_int = arrayToInt(&received_values[1]);
      button_state[1] = false;
      if (boolean_as_int == 1) {
        button_state[1] = true;
      }

    } else if (address == 13) {
      int boolean_as_int = arrayToInt(&received_values[1]);
      button_state[2] = false;
      if (boolean_as_int == 1) {
        button_state[2] = true;
      }

    } else if (address == 14) {
      int tmp = arrayToInt(&received_values[1]);
      door_start_time = *((long*)&tmp);

    } else if (address == 15) {
      door_position = arrayToInt(&received_values[1]);

    } else if (address == 16) {
      encoder_value = arrayToInt(&received_values[1]);

    } else if (address == 17) {
      int tmp = arrayToInt(&received_values[1]);
      encoder_time = *((long*)&tmp);

    } else if (address == 18) {
      int tmp = arrayToInt(&received_values[1]);
      encoder_ticks = *((long*)&tmp);

    } else if (address == 19) {
      int tmp = arrayToInt(&received_values[1]);
      encoder_speed = *((float*)&tmp);

    } else if (address == 20) {
      int boolean_as_int = arrayToInt(&received_values[1]);
      encoder_overspeed = false;
      if (boolean_as_int == 1) {
        encoder_overspeed = true;
      }

    }
  } else if (nbr_of_received_bytes == 1) {
    request_queue[queue_end++] = received_values[0];
    if (queue_end == 100) {
      queue_end = 0;
    }
  }
}

// i2c request event
void requestEvent() {

  if (state != testc_state) {
    queue_start = 0;
    queue_end = 0;
  }

  if (queue_start < queue_end) {
    int requested_address = request_queue[queue_start++];
    if (queue_start == 100) {
      queue_start = 0;
    }
    if (requested_address == 0) {
      char val[4];
      intToCharArray(val, 1);
      Wire.write(val, 4);

    } else if (requested_address == 1) { // OperationState state
      char val[4];
      intToCharArray(val, state);
      Wire.write(val, 4);

    } else if (requested_address == 2) { // OperationState last_state
      char val[4];
      intToCharArray(val, last_state);
      Wire.write(val, 4);

    } else if (requested_address == 3) { // long state_time
      char val[4];
      intToCharArray(val, *((int*)&state_time));
      Wire.write(val, 4);

    } else if (requested_address == 4) { // long state_cycle
      char val[4];
      intToCharArray(val, *((int*)&state_cycle));
      Wire.write(val, 4);

    } else if (requested_address == 5) { // int level_position
      char val[4];
      intToCharArray(val, level_position);
      Wire.write(val, 4);

    } else if (requested_address == 6) { // int level_target
      char val[4];
      intToCharArray(val, level_target);
      Wire.write(val, 4);

    } else if (requested_address == 7) { // PositionState level_position_state[0]
      char val[4];
      intToCharArray(val, level_position_state[0]);
      Wire.write(val, 4);

    } else if (requested_address == 8) { // PositionState level_position_state[1]
      char val[4];
      intToCharArray(val, level_position_state[1]);
      Wire.write(val, 4);

    } else if (requested_address == 9) { // PositionState level_position_state[2]
      char val[4];
      intToCharArray(val, level_position_state[2]);
      Wire.write(val, 4);

    } else if (requested_address == 10) { // int last_blocked_level
      char val[4];
      intToCharArray(val, last_blocked_level);
      Wire.write(val, 4);

    } else if (requested_address == 11) { // boolean button_state[0]
      char val[4];
      int bool_as_int = 0;
      if (button_state[0] == 1) {
        bool_as_int = 1;
      }
      intToCharArray(val, bool_as_int);
      Wire.write(val, 4);
      Serial.println("send 0");

    } else if (requested_address == 12) { // boolean button_state[1]
      char val[4];
      int bool_as_int = 0;
      if (button_state[1] == 1) {
        bool_as_int = 1;
      }
      intToCharArray(val, bool_as_int);
      Wire.write(val, 4);
      Serial.println("send 1");

    } else if (requested_address == 13) { // boolean button_state[2]
      char val[4];
      int bool_as_int = 0;
      if (button_state[2] == 1) {
        bool_as_int = 1;
      }
      intToCharArray(val, bool_as_int);
      Wire.write(val, 4);
      Serial.println("send 2");

    } else if (requested_address == 14) { // long door_start_time
      char val[4];
      intToCharArray(val, *((int*)&door_start_time));
      Wire.write(val, 4);

    } else if (requested_address == 15) { // int door_position
      char val[4];
      intToCharArray(val, door_position);
      Wire.write(val, 4);

    } else if (requested_address == 16) { // int encoder_value
      char val[4];
      intToCharArray(val, encoder_value);
      Wire.write(val, 4);

    } else if (requested_address == 17) { // long encoder_time
      char val[4];
      intToCharArray(val, *((int*)&encoder_time));
      Wire.write(val, 4);

    } else if (requested_address == 18) { // long encoder_ticks
      char val[4];
      intToCharArray(val, *((int*)&encoder_ticks));
      Wire.write(val, 4);

    } else if (requested_address == 19) { // float encoder_speed
      char val[4];
      intToCharArray(val, *((int*)&encoder_speed));
      Wire.write(val, 4);

    } else if (requested_address == 20) { // boolean encoder_overspeed
      char val[4];
      int bool_as_int = 0;
      if (button_state[1] == 1) {
        bool_as_int = 1;
      }
      intToCharArray(val, encoder_overspeed);
      Wire.write(val, 4);

    } else if (requested_address == 21) { // MotorDirection motor_direction
      char val[4];
      intToCharArray(val, motor_direction);
      Wire.write(val, 4);

    } else if (requested_address == 22) { // MotorSpeed motor_speed
      char val[4];
      intToCharArray(val, motor_speed);
      Wire.write(val, 4);

    }
  }
}
#endif


