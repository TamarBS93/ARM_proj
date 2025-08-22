#include "uarts.h"
/*
 *
 * UART2_TX                 UART5_RX
 * PA3 RX (CN9) <---------- PC12 TX (CN8)
 * PD5 TX (CN9) ----------> PD2 RX (CN8)
 *
 */

#define UART_SENDER 		(&huart2)
#define UART_RECEIVER 		(&huart5)

result_pro_t uart_testing(test_command_t* command){


	uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
	uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

	result_pro_t response;
	HAL_StatusTypeDef rx_status, tx_status;

	if (command == NULL) {
        printf("UART_TEST: Received NULL command pointer. Skipping.\n\r");
        response.test_result = TEST_ERR;
        return response;
	}

	response.test_id = command->test_id;
	// Copy pattern to TX buffer
    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

	for(uint8_t i=0 ; i< command->iterations ; i++){
	    printf("UART_TEST: Iteration %u/%u -\n\r", i + 1, command->iterations);
	    memset(rx_buffer, 0, command->bit_pattern_length);

	    // --- 1. START RECEIVE DMA ---
	    HAL_StatusTypeDef rx_status = HAL_UART_Receive_IT(UART_RECEIVER, rx_buffer, command->bit_pattern_length);
	    if (rx_status != HAL_OK) {
	        printf("Failed to start receive: %d\n\r", rx_status);
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        return response;
	    }

	    // --- 2. TRANSMIT a block of data via DMA ---
	    HAL_StatusTypeDef tx_status = HAL_UART_Transmit_IT(UART_SENDER, tx_buffer, command->bit_pattern_length);
	    if (tx_status != HAL_OK) {
	        printf("Failed to send on UART sender: %d\n\r", tx_status);
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        HAL_UART_DMAStop(UART_RECEIVER); // Stop the pending receive
	        return response;
	    }

	    // --- 3. WAIT FOR BOTH TX AND RX DMA COMPLETION ---
	    if (xSemaphoreTake(UartTxHandle, TIMEOUT) != pdPASS) {
	         printf("fail to get TxSemaphore\n\r");
	         response.test_result = TEST_FAIL;
	         vPortFree(command);
	         HAL_UART_DMAStop(UART_RECEIVER); // Stop the pending receive
	         return response;
	    }

	    if (xSemaphoreTake(UartRxHandle, TIMEOUT) != pdPASS) {
	         printf("fail to get RxSemaphore\n\r");
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        HAL_UART_DMAStop(UART_RECEIVER); //Stop the stuck receive
	        return response;
	    }

	    // --- 4. COMPARE SENT vs. RECEIVED data ---
	    if (command->bit_pattern_length > 100) {
			printf("bit_pattern_length more than 100\n\r");

			// Use CRC comparison for large data
			uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
			uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
			if (sent_crc != received_crc) {
				printf("UART_TEST: CRC mismatch on iteration %u. Sent CRC: 0x%lX, Received CRC: 0x%lX\n\r",
					   i + 1, sent_crc, received_crc);
				response.test_result = TEST_FAIL;
				vPortFree(command);
				return response;
			}
	    }
	    else {
			int comp = memcmp(tx_buffer, rx_buffer, command->bit_pattern_length);
			if (comp != 0) {
				printf("Data mismatch on iteration %u.\n\r", i + 1);
				printf("Sent: %.*s\n\r", command->bit_pattern_length, tx_buffer);
				printf("Recv: %.*s\n\r", command->bit_pattern_length, rx_buffer);
				response.test_result = TEST_FAIL;
				vPortFree(command);
				return response;
			}
	    }
	    printf("Data Match on iteration %u.\n\r", i + 1);

        osDelay(10); // Small delay between iterations to prevent overwhelming the UUT or the system
	}
    response.test_result = TEST_PASS;
    vPortFree(command);
    return response;
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (huart->Instance == UART_SENDER->Instance) // Check the instance of your sender UART
    {
        xSemaphoreGiveFromISR(UartTxHandle, &xHigherPriorityTaskWoken);
        printf("TX callback fired and freed the semaphore\n\r");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (huart->Instance == UART_RECEIVER->Instance) // Check the instance of your receiver UART
    {
        xSemaphoreGiveFromISR(UartRxHandle, &xHigherPriorityTaskWoken);
        printf("RX callback fired and freed the semaphore\n\r");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
