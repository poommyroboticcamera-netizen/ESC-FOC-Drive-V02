/*
 * Custom VESC target for the V.Phase2_v02 controller.
 * Nominal DC bus: 24 V. See README_TH.md before flashing.
 */

#ifndef HW_BLDC_LITE_24_H_
#define HW_BLDC_LITE_24_H_

#define BLDC_LITE_24
#define BLDC_LITE_FOC_3SHUNT
#define HW_NAME                         "BLDC_LITE_24"
#define BLDC_NOMINAL_VBUS               24.0
#define BLDC_DEFAULT_VIN_MIN            10.0
#define BLDC_DEFAULT_VIN_MAX            30.0
#define BLDC_BATTERY_CUT_START          20.0
#define BLDC_BATTERY_CUT_END            18.0
#define BLDC_HW_VIN_MIN                 8.0
#define BLDC_HW_VIN_MAX                 32.0

#include "hw_bldc_lite_core.h"

#endif /* HW_BLDC_LITE_24_H_ */
