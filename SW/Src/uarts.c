#include "uarts.h"
/*
 *
 * UART2_TX                 UART4_RX
 * PD6 RX (CN9) <---------- PA0 TX (CN10)
 * PD5 TX (CN9) ----------> PC11 RX (CN8)
 * DMA1 stream6             DMA1 stream2
 */

#define UART_SENDER 		(&huart2)
#define UART_RECEIVER 		(&huart4)
uint8_t g_rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

result_pro_t uart_testing(test_command_t* command){

	uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
	uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};


	result_pro_t response;
	HAL_StatusTypeDef rx_status, tx_status;

	if (command == NULL) {
        printf("UART_TEST: Received NULL command pointer. Skipping.\n\r"); // Debug printf
        response.test_result = TEST_ERR;
        return response;
	}

	response.test_id = command->test_id;
	// Copy pattern to TX buffer
    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

    for(uint8_t i=0 ; i< command->iterations ; i++){
        printf("UART_TEST: Iteration %u/%u:\n\r", i + 1, command->iterations);
        memset(rx_buffer, 0, command->bit_pattern_length);

        // RECEIVER start to RECEIVE DMA
        rx_status = HAL_UART_Receive_DMA(UART_RECEIVER, g_rx_buffer, command->bit_pattern_length);
        if (rx_status != HAL_OK) {
            printf("Receiver Failed to start receive: %d\n\r", rx_status);
            response.test_result = TEST_FAIL;
            vPortFree(command);
            return response;
        }
        // Arm sender receive before receiver transmits back
        if (HAL_UART_Receive_IT(UART_SENDER, rx_buffer, command->bit_pattern_length) != HAL_OK) {
            printf("Sender Failed to start receive back\n\r");
            response.test_result = TEST_FAIL;
            vPortFree(command);
            return response;
        }

        // SENDER TRANSMIT a block of data via DMA
        tx_status = HAL_UART_Transmit_DMA(UART_SENDER, tx_buffer, command->bit_pattern_length);
        if (tx_status != HAL_OK) {
            printf("Failed to send on UART sender: %d\n\r", tx_status);
            response.test_result = TEST_FAIL;
            vPortFree(command);
            HAL_UART_DMAStop(UART_RECEIVER);
            return response;
        }

        // WAIT FOR RECEIVER RX COMPLETION
        if (xSemaphoreTake(UartRxHandle, TIMEOUT) != pdPASS) {
            printf("fail to get RxSemaphore\n\r");
            response.test_result = TEST_FAIL;
            vPortFree(command);
            HAL_UART_DMAStop(UART_SENDER);
            HAL_UART_DMAStop(UART_RECEIVER);
            return response;
        }

//        // Now transmitter echoes back
//        if (HAL_UART_Transmit_DMA(UART_RECEIVER, rx_buffer, command->bit_pattern_length) != HAL_OK) {
//            printf("Receiver Fail to start Transmitting back\n\r");
//            response.test_result = TEST_FAIL;
//            vPortFree(command);
//            return response;
//        }

        // WAIT FOR TX COMPLETION
        if (xSemaphoreTake(UartTxHandle, TIMEOUT) != pdPASS) {
             printf("fail to get TxSemaphore\n\r");
             response.test_result = TEST_FAIL;
             vPortFree(command);
             HAL_UART_DMAStop(UART_RECEIVER);
             HAL_UART_DMAStop(UART_SENDER);
             return response;
        }

	    // COMPARE SENT vs. RECEIVED data
	    if (command->bit_pattern_length > 100) {
//			printf("bit_pattern_length more than 100\n\r"); // Debug printf

			// Use CRC comparison for large data
			uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
			uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
			if (sent_crc != received_crc) {
				// Debug printf
//				printf("UART_TEST: CRC mismatch on iteration %u. Sent CRC: 0x%lX, Received CRC: 0x%lX\n\r",
//					   i + 1, sent_crc, received_crc);
				response.test_result = TEST_FAIL;
				vPortFree(command);
				return response;
			}
	    }
	    else {
			int comp = memcmp(tx_buffer, rx_buffer, command->bit_pattern_length);
			if (comp != 0) {
//				// Debug printf
//				printf("Data mismatch on iteration %u.\n\r", i + 1);
//				printf("Sent: %.*s\n\r", command->bit_pattern_length, tx_buffer);
//				printf("Recv: %.*s\n\r", command->bit_pattern_length, rx_buffer);
				response.test_result = TEST_FAIL;
				vPortFree(command);
				return response;
			}
	    }
	    printf("Data Match on iteration %u.\n\r", i + 1); // Debug printf

        osDelay(10); // Small delay between iterations to prevent overwhelming the UUT or the system
	}
    response.test_result = TEST_PASS;
    vPortFree(command);
    return response;
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (huart->Instance == UART_RECEIVER->Instance)
    {
        printf("Receiver Tx callback fired\n\r");
    }
    else if (huart->Instance == UART_SENDER->Instance)
    {
        xSemaphoreGiveFromISR(UartTxHandle, &xHigherPriorityTaskWoken);
        printf("Sender Tx callback fired\n\r");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (huart->Instance == UART_RECEIVER->Instance)
    {
    	 if (HAL_UART_Transmit_IT(UART_RECEIVER, UART_RECEIVER->pRxBuffPtr, UART_RECEIVER->RxXferSize) != HAL_OK){
             printf("Receiver Failed to start transmit back\n\r");
    	 }
        printf("Receiver Rx callback fired \n\r");
    }
    else if (huart->Instance == UART_SENDER->Instance)
    {
        xSemaphoreGiveFromISR(UartRxHandle, &xHigherPriorityTaskWoken);
        printf("Sender Rx callback fired (received back)\n\r");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
