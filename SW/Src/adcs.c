#include "adcs.h"

/*
 * ADC                    DAC
 * PA0 (CN10) <---------- PA4 (CN7)
 */

/*
 * @brief Performs a test on the ADC peripheral using the command protocol.
 * @param command: A pointer to the test_command_t struct.
 * @retval result_t: The result of the test (TEST_PASS or TEST_FAIL).
 */
result_pro_t adc_testing(test_command_t* command){

	result_pro_t response;

	uint32_t adc_value;
    int32_t difference;
    HAL_StatusTypeDef status;

    // Check for valid command and bit pattern length
	if (command == NULL) {
        printf("ADC_TEST: Received NULL command pointer. Skipping.\n\r");
        response.test_result = TEST_ERR;
        return response;
	}
	response.test_id = command->test_id;

	uint32_t expected_adc_result = command->bit_pattern[0];
	uint32_t adc_tolerance = (uint32_t)(expected_adc_result * TOLERANCE_PERCENT);

    status = HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
    if (status != HAL_OK) {
        printf("Error: Failed to start DAC conversion. Status: %d\n\r", status);
    	response.test_result = TEST_FAIL;
        vPortFree(command);
        return response;
    }

	for(uint8_t i=0 ; i< command->iterations ; i++){

		if(command->iterations < command->bit_pattern_length){
			// Extract the 8-bit expected ADC value from the command's bit pattern
		    expected_adc_result = command->bit_pattern[i];
		    // Define a tolerance based on the expected result.
		    adc_tolerance = (uint8_t)(expected_adc_result * TOLERANCE_PERCENT);
		}

	    // Set value to DAC and run
	    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_8B_R, expected_adc_result);
	    HAL_Delay(1); // allow DAC to settle

	    // Start ADC conversion
	    status = HAL_ADC_Start_IT(&hadc1);
	    if (status != HAL_OK) {
	        printf("Error: Failed to start ADC conversion. Status: %d\n\r", status);
	    	HAL_ADC_Stop(&hadc1);
	    	response.test_result = TEST_FAIL;
	        vPortFree(command);
	        return response;
	    }

	    // waiting for the ADC conversion to complete and give a semaphore
	    if (xSemaphoreTake(AdcSemHandle, HAL_MAX_DELAY) == pdPASS){
		  // Get the converted value
		  adc_value = HAL_ADC_GetValue(&hadc1);
		} // end of ADC conversion
		else{
	         printf("ADC semaphore acquire failed or timed out\n\r");
	         HAL_ADC_Stop(&hadc1);
	         response.test_result = TEST_FAIL;
	         vPortFree(command);
	         return response;
		}

		// Compare the result with the expected value, within a tolerance
		difference = adc_value - expected_adc_result;
		difference = (difference < 0) ? -difference : difference; //absolute value of the difference

		if (difference <= adc_tolerance) {
		  printf("ADC value is within tolerance for iteration %u\n\r", i+1);
		  printf("Expected value=%d >> ADC value =%ld \n\r", expected_adc_result, adc_value);
		} else {
		  printf("Test failed on iteration %u- Expected Value: %u, ADC value: %lu.\n\r",i+1, expected_adc_result, adc_value);
		  HAL_ADC_Stop(&hadc1);
		  response.test_result = TEST_FAIL;
		  vPortFree(command);
		  return response;
		}
		// Stop the ADC conversion
		status = HAL_ADC_Stop(&hadc1);
		if (status != HAL_OK) {
			printf("Warning: Failed to stop ADC conversion. Status: %d\n\r", status);
	         response.test_result = TEST_FAIL;
	         vPortFree(command);
	         return response;
		}
	} // end of iterations

	response.test_result = TEST_PASS;
	vPortFree(command);
	return response;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(AdcSemHandle, &xHigherPriorityTaskWoken);
	printf("ADC complete callback fired and gave a semaphore\n\r");
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
