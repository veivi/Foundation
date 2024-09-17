/*
 * This file is part of INAV.
 *
 * INAV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * INAV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with INAV.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <platform.h>
#include "config/config_master.h"
#include "config/feature.h"
#include "flight/mixer.h"
#include "io/serial.h"
#include "telemetry/telemetry.h"
#include "drivers/pwm_mapping.h"

// alternative defaults settings for MATEKF405SE targets
void targetConfiguration(void)
{
  featureSet(FEATURE_PWM_OUTPUT_ENABLE); // enable PWM outputs by default
  mixerConfigMutable()->platformType = PLATFORM_AIRPLANE;   // default mixer to Airplane

  primaryMotorMixerMutable(0)->throttle = 0.0f;
      
  servoConfigMutable()->servo_protocol = SERVO_TYPE_PWM;
  servoConfigMutable()->servoPwmRate = 50;

  for (int i = 0; i < 9; i++) {
    customServoMixersMutable(i)->targetChannel = i;
    customServoMixersMutable(i)->rate = 100;
  }
}
