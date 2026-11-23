/**
 *    ||          ____  _ __
 * +------+      / __ )(_) /_______________ _____  ___
 * | 0xBC |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * +------+    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *  ||  ||    /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * Crazyflie control firmware
 *
 * Copyright (C) 2011-2016 Bitcraze AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * power_distribution_stock.c - Crazyflie stock power distribution code
 */

#include <string.h>

#include "power_distribution.h"

#include <string.h>
#include "log.h"
#include "param.h"
#include "num.h"
#include "platform.h"
#include "motors.h"
#include "system.h"
#define DEBUG_MODULE "PWR_DIST"
#include "debug_cf.h"

static bool motorSetEnable = false;
static bool motorInitSucceeded = false;

static struct {
  uint32_t m1;
  uint32_t m2;
  uint32_t m3;
  uint32_t m4;
} motorPower;

static struct {
  uint16_t m1;
  uint16_t m2;
  uint16_t m3;
  uint16_t m4;
} motorPowerSet;

#ifndef DEFAULT_IDLE_THRUST
#define DEFAULT_IDLE_THRUST 0
#endif

static uint32_t idleThrust = DEFAULT_IDLE_THRUST;

#ifdef CONFIG_KUBIT_PHONE_BENCH
#include <stdio.h>
#include <stdlib.h>
/* Serial-console direct values (props OFF). Clamped again by the motor driver cap. */
static volatile bool kubitSerialDirect = false;
static volatile uint16_t kubitSerialM[4];

static void kubitPrintState(void)
{
  DEBUG_PRINTW("KUBIT: cap=%d%% armed=%d latched=%d(runcap=%d) serialDirect=%d m=%u,%u,%u,%u",
               kubitMotorsGetCapPercent(), (int)systemIsArmed(), (int)kubitMotorsStopLatched(),
               (int)kubitMotorsLatchIsRunCap(), (int)kubitSerialDirect,
               kubitSerialM[0], kubitSerialM[1], kubitSerialM[2], kubitSerialM[3]);
}

static bool kubitSetValue(int idx, long v)
{
  if (v < 0 || v > 65535) {
    return false;
  }
  kubitSerialM[idx] = (uint16_t)v;
  return true;
}

/* Console lines: a, d, m <1-4> <0-65535>, all <0-65535>, ?  ('s' is handled by the driver). */
static bool kubitConsole(const char *line)
{
  int n;
  long v;
  if (line[0] == '?' && line[1] == '\0') {
    DEBUG_PRINTW("KUBIT cmds: a=arm/clear run-cap, d=disarm, c <pct>=duty cap, m <1-4> <v>, all <v>, s=latch off (reset), ?");
    kubitPrintState();
    return true;
  }
  if (line[0] == 'a' && line[1] == '\0') {
    if (!kubitMotorsClearRunCapLatch()) {
      DEBUG_PRINTW("KUBIT: serial-stop latch is set; reset the board to arm");
      return true;
    }
    /* Re-arming never resumes old serial values: start from zero. */
    kubitSerialDirect = false;
    for (int i = 0; i < 4; i++) {
      kubitSerialM[i] = 0;
    }
    systemSetArmed(true);
    DEBUG_PRINTW("KUBIT: ARMED. Phone thrust now drives all four motors (cap applies)");
    kubitPrintState();
    return true;
  }
  if (line[0] == 'd' && line[1] == '\0') {
    kubitMotorsResetCap();
    kubitSerialDirect = false;
    for (int i = 0; i < 4; i++) {
      kubitSerialM[i] = 0;
    }
    systemSetArmed(false);
    DEBUG_PRINTW("KUBIT: DISARMED, motors 0");
    return true;
  }
  if (sscanf(line, "c %d", &n) == 1) {
    if (!kubitMotorsSetCapPercent(n)) {
      DEBUG_PRINTW("KUBIT: c <5-%d> percent (build ceiling)", CONFIG_KUBIT_MOTOR_CAP_CEILING_PERCENT);
    } else {
      DEBUG_PRINTW("KUBIT: duty cap now %d%% (resets to default on d or reboot)", n);
    }
    kubitPrintState();
    return true;
  }
  if (sscanf(line, "m %d %ld", &n, &v) == 2 || sscanf(line, "m%d %ld", &n, &v) == 2) {
    if (n < 1 || n > 4 || !kubitSetValue(n - 1, v)) {
      DEBUG_PRINTW("KUBIT: m <1-4> <0-65535>");
      return true;
    }
    kubitSerialDirect = true;
    kubitPrintState();
    return true;
  }
  if (sscanf(line, "all %ld", &v) == 1) {
    if (v < 0 || v > 65535) {
      DEBUG_PRINTW("KUBIT: all <0-65535>");
      return true;
    }
    for (int i = 0; i < 4; i++) {
      kubitSerialM[i] = (uint16_t)v;
    }
    kubitSerialDirect = true;
    kubitPrintState();
    return true;
  }
  return false;
}
#endif

void powerDistributionInit(void)
{
  motorInitSucceeded = motorsInit(platformConfigGetMotorMapping());
#ifdef CONFIG_KUBIT_PHONE_BENCH
  kubitMotorsSetConsoleHandler(kubitConsole);
#endif
}

bool powerDistributionTest(void)
{
  bool pass = motorInitSucceeded;

  pass &= motorsTest();

  return pass;
}

#define limitThrust(VAL) limitUint16(VAL)

void powerStop()
{
  motorsSetRatio(MOTOR_M1, 0);
  motorsSetRatio(MOTOR_M2, 0);
  motorsSetRatio(MOTOR_M3, 0);
  motorsSetRatio(MOTOR_M4, 0);
}

void powerDistribution(const control_t *control)
{
  #ifdef QUAD_FORMATION_X
    int16_t r = control->roll / 2.0f;
    int16_t p = control->pitch / 2.0f;
    motorPower.m1 = limitThrust(control->thrust - r + p + control->yaw);
    motorPower.m2 = limitThrust(control->thrust - r - p - control->yaw);
    motorPower.m3 =  limitThrust(control->thrust + r - p + control->yaw);
    motorPower.m4 =  limitThrust(control->thrust + r + p - control->yaw);
  #else // QUAD_FORMATION_NORMAL
    motorPower.m1 = limitThrust(control->thrust + control->pitch +
                               control->yaw);
    motorPower.m2 = limitThrust(control->thrust - control->roll -
                               control->yaw);
    motorPower.m3 =  limitThrust(control->thrust - control->pitch +
                               control->yaw);
    motorPower.m4 =  limitThrust(control->thrust + control->roll -
                               control->yaw);
  #endif

#ifdef CONFIG_KUBIT_PHONE_BENCH
  if (kubitSerialDirect)
  {
    /* Serial console values; armed state is already checked by the stabilizer. */
    motorsSetRatio(MOTOR_M1, kubitSerialM[0]);
    motorsSetRatio(MOTOR_M2, kubitSerialM[1]);
    motorsSetRatio(MOTOR_M3, kubitSerialM[2]);
    motorsSetRatio(MOTOR_M4, kubitSerialM[3]);
  }
  else if (motorSetEnable)
  {
    motorsSetRatio(MOTOR_M1, motorPowerSet.m1);
    motorsSetRatio(MOTOR_M2, motorPowerSet.m2);
    motorsSetRatio(MOTOR_M3, motorPowerSet.m3);
    motorsSetRatio(MOTOR_M4, motorPowerSet.m4);
  }
  else
  {
    /* Phone thrust, equal on all four motors, no attitude mixing. */
    motorPower.m1 = motorPower.m2 = motorPower.m3 = motorPower.m4 =
      limitThrust(control->thrust);
    motorsSetRatio(MOTOR_M1, motorPower.m1);
    motorsSetRatio(MOTOR_M2, motorPower.m2);
    motorsSetRatio(MOTOR_M3, motorPower.m3);
    motorsSetRatio(MOTOR_M4, motorPower.m4);
  }
#else
  if (motorSetEnable)
  {
    motorsSetRatio(MOTOR_M1, motorPowerSet.m1);
    motorsSetRatio(MOTOR_M2, motorPowerSet.m2);
    motorsSetRatio(MOTOR_M3, motorPowerSet.m3);
    motorsSetRatio(MOTOR_M4, motorPowerSet.m4);
  }
  else
  {
    if (motorPower.m1 < idleThrust) {
      motorPower.m1 = idleThrust;
    }
    if (motorPower.m2 < idleThrust) {
      motorPower.m2 = idleThrust;
    }
    if (motorPower.m3 < idleThrust) {
      motorPower.m3 = idleThrust;
    }
    if (motorPower.m4 < idleThrust) {
      motorPower.m4 = idleThrust;
    }

    motorsSetRatio(MOTOR_M1, motorPower.m1);
    motorsSetRatio(MOTOR_M2, motorPower.m2);
    motorsSetRatio(MOTOR_M3, motorPower.m3);
    motorsSetRatio(MOTOR_M4, motorPower.m4);
  }
#endif
}

PARAM_GROUP_START(motorPowerSet)
PARAM_ADD(PARAM_UINT8, enable, &motorSetEnable)
PARAM_ADD(PARAM_UINT16, m1, &motorPowerSet.m1)
PARAM_ADD(PARAM_UINT16, m2, &motorPowerSet.m2)
PARAM_ADD(PARAM_UINT16, m3, &motorPowerSet.m3)
PARAM_ADD(PARAM_UINT16, m4, &motorPowerSet.m4)
PARAM_GROUP_STOP(motorPowerSet)

PARAM_GROUP_START(powerDist)
PARAM_ADD(PARAM_UINT32, idleThrust, &idleThrust)
PARAM_GROUP_STOP(powerDist)

LOG_GROUP_START(motor)
LOG_ADD(LOG_UINT32, m1, &motorPower.m1)
LOG_ADD(LOG_UINT32, m2, &motorPower.m2)
LOG_ADD(LOG_UINT32, m3, &motorPower.m3)
LOG_ADD(LOG_UINT32, m4, &motorPower.m4)
LOG_GROUP_STOP(motor)
