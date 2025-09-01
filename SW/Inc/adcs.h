#ifndef ADCS_P_H_
#define ADCS_P_H_

#include <stdint.h>
#include "cmsis_os.h"

#include "FreeRTOS.h"
#include "semphr.h" // For semaphore-specific functions and types like SemaphoreHandle_t

#include "stm32f7xx_hal.h" // General HAL header, often includes peripheral specific ones

#include "project_header.h"

extern ADC_HandleTypeDef hadc1;
extern DAC_HandleTypeDef hdac;

extern osSemaphoreId_t AdcSemHandle;

#define TOLERANCE_PERCENT 0.1f
//#define TOLERANCE_ALLOWED 5


// Define the expected ADC result.
// This value depends on your known input voltage and the ADC reference voltage.
// For example, if you have a 3.3V reference and a 1.65V input:
// Expected_Result = (1.65 / 3.3) * 4095 = 2047.
//#define EXPECTED_ADC_RESULT 2047
//#define adc_tolerance 5 // A small tolerance for comparison

result_pro_t adc_testing(test_command_t*);

#endif /* ADCS_P_H_ */
