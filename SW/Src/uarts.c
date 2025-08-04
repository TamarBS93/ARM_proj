#include "uarts.h"

#define TIMEOUT 	1000 	// ticks (30  millis).

#define UART_SENDER 		(&huart2)
#define UART_RECEIVER 		(&huart6)

result_pro_t uart_testing(test_command_t* command){


	uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
	uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

	result_pro_t response;
	HAL_StatusTypeDef status;

	if (command == NULL) {
        printf("UART_TEST: Received NULL command pointer. Skipping.\n");
        response.test_result = TEST_ERR;
        return response;
	}

	response.test_id = command->test_id;

	// Copy pattern to TX buffer
    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

	for(uint8_t i=0 ; i< command->iterations ; i++){
		memset(rx_buffer, 0, sizeof(rx_buffer)); // Clear receive buffer

	    status = HAL_UART_Receive_DMA(UART_RECEIVER, rx_buffer, sizeof(rx_buffer));
	    if (status != HAL_OK) {
	        printf("Failed to start receive DMA: %d, error 0x%"PRIX32"\r\n", status, HAL_UART_GetError(UART_RECEIVER));
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        return response;
	    }

        // --- 1. TRANSMIT a block of data via DMA ---
		status = HAL_UART_Transmit_DMA(UART_SENDER, tx_buffer, command->bit_pattern_length);
		if (status != HAL_OK) {
			printf("Failed to sendIT on UART sender: %d, error 0x%"PRIX32"\r\n", status, HAL_UART_GetError(UART_SENDER));
	        response.test_result = TEST_FAIL;
	        return response;
		}

//        // Wait for the TX DMA transfer to complete (semaphore is given in the TxCpltCallback)
//        // --- Wait for TX to complete
//        if (xSemaphoreTake(xUartTxSemaphoreHandle, pdMS_TO_TICKS(2000)) != pdPASS) {
//             printf("TX DMA timeout\n");
//             response.test_result = TEST_FAIL;
//             vPortFree(command);
//             return response;
//        }
//        if (xSemaphoreTake(xUartRxSemaphoreHandle, pdMS_TO_TICKS(2000)) != pdPASS) { // 2 second timeout
//            printf("RX DMA timeout\n");
//            response.test_result = TEST_FAIL;
//            vPortFree(command);
//            return response;
////            // --- 2. RECEIVE the block of data via DMA ---
////    		status = HAL_UART_Receive_DMA(UART_RECEIVER, rx_buffer, command->bit_pattern_length);
////    		if (status != HAL_OK) {
////    			printf("Failed to receiveIT on UART receiver: %d, error 0x%"PRIX32"\r\n", status, HAL_UART_GetError(UART_RECEIVER));
////    	        response.test_result = TEST_FAIL;
////    	        vPortFree(command);
////    	        return response;
////    		}
//        }
//        else{
//			printf("Failed to receive a xUartRxSemaphore Token\n");
//        }

        // --- 3. COMPARE SENT vs. RECEIVED data ---
        if (command->bit_pattern_length > 100) {
        	printf("bit_pattern_length more than 100");

//            // Use CRC comparison for large data
//            uint32_t sent_crc = calculate_crc32(tx_buffer, command->bit_pattern_length);
//            uint32_t received_crc = calculate_crc32(rx_buffer, command->bit_pattern_length);
//            if (sent_crc == received_crc) {
//                response.test_result = PASS;
//                return response;
//            } else {
//                printf("UART_TEST: CRC mismatch on iteration %u. Sent CRC: 0x%lX, Received CRC: 0x%lX\n",
//                       i + 1, sent_crc, received_crc);
//                response.test_result = FAIL;
//                return response;            }
        } else {
            // Use memcmp for smaller data
        	int comp = memcmp(tx_buffer, rx_buffer, command->bit_pattern_length);
            if (comp != 0)
            {
            	printf("memcmp result: %d\n", comp);

                printf("UART_TEST: Data mismatch on iteration %u.\n", i + 1);
                // debug print mismatch details for small data
                printf("Sent: %.*s\n", command->bit_pattern_length, tx_buffer);
                printf("Recv: %.*s\n", command->bit_pattern_length, rx_buffer);

                response.test_result = TEST_FAIL;
                vPortFree(command);
                return response;
            }
            else{
                printf("UART_TEST: Data Match on iteration %u.\n", i + 1);
            }
        }

        osDelay(10); // Small delay between iterations to prevent overwhelming the UUT or the system
	}
    response.test_result = TEST_PASS;
    vPortFree(command);
    return response;
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    printf("TX callback fired\n");

    if (huart->Instance == UART_SENDER->Instance) // Check the instance of your sender UART
    {
        xSemaphoreGiveFromISR(xUartTxSemaphoreHandle, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    printf("RX callback fired\n");

    if (huart->Instance == UART_RECEIVER->Instance) // Check the instance of your receiver UART
    {
        xSemaphoreGiveFromISR(xUartRxSemaphoreHandle, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
