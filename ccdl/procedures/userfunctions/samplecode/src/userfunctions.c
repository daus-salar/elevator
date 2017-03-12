/*************************************************************************
 *
 * Copyright(C) 2010-2016 Razorcat Development GmbH. All Rights Reserved.
 *
 *************************************************************************
 */

/**\file
 *    Sample user functions implemention
 *    provided with the CCDL installation package.
 */

/* Always include own header first */
#include "userfunctions.h"

#include <string.h>
#include <stdio.h>

#define true 1
#define false 0
#define boolean unsigned char

/*=========================================================================*/


// Encoder related constants
static const int ENCODER_RESOLUTION = 24;
static const long MILLI_PER_SECOND = 1000;
static const long REFERENCE_TURN = 100;
static const long STOP_TIME = 100;

// Global variables for encoder interpretion
int encoder_value = 0;
long encoder_time = 0;
long encoder_ticks = 0;
long encoder_speed = 0;
boolean encoder_overspeed = false;

extern ccdl_te_parameterHandle _TDLP_DI_ROTARY_ENCODER_A;
extern ccdl_te_parameterHandle _TDLP_DI_ROTARY_ENCODER_B;

static int readEncoderValue();
static void transferEncoderInput(tdl_measureSpeed_t * data);

double tdl_measureSpeed(ccdl_StatementReturnType * rp, const char * module, tdl_measureSpeed_t * data)
{
	transferEncoderInput(data);

	// Will be called every 3 ms	
	data->counter += 3;
	
	rp->code = RC_FINISHED;

	return encoder_speed;
}


static int readEncoderValue() {
  int a = ccdl_te_getParValue(_TDLP_DI_ROTARY_ENCODER_A) ? 1 : 0;
  int b = ccdl_te_getParValue(_TDLP_DI_ROTARY_ENCODER_B) ? 2 : 0;
  return a + b;
}


static void transferEncoderInput(tdl_measureSpeed_t * data) {
  // Encoder Input
  int newValue = readEncoderValue();
  long newTime = data->counter;

  if (encoder_time == newTime) {
    return;
  }

  if (newValue != encoder_value) {
    boolean oldA = (encoder_value & 1) != 0;
    boolean oldB = (encoder_value & 2) != 0;
    boolean newA = (newValue & 1) != 0;
    boolean newB = (newValue & 2) != 0;

    if (oldA != newA && oldB != newB) {
      encoder_overspeed = true;
      encoder_value = newValue;
      encoder_time = newTime;
      encoder_ticks += 2;
      encoder_speed = 0;
      return;
    }

    encoder_overspeed = false;
    encoder_ticks++;

    long timeDelta = newTime - encoder_time;
    long t = ENCODER_RESOLUTION * timeDelta;
    encoder_speed = REFERENCE_TURN * MILLI_PER_SECOND / t;

    if ((encoder_value == 0 && newValue == 1) || (encoder_value == 1 && newValue == 3) ||
        (encoder_value == 3 && newValue == 2) || (encoder_value == 2 && newValue == 0)) {
      encoder_speed *= -1;
    }

    encoder_time = newTime;
    encoder_value = newValue;
  } else if (newTime > encoder_time + STOP_TIME) {
    encoder_overspeed = false;
    encoder_value = newValue;
    encoder_time = newTime;
    encoder_speed = 0;
  }
}