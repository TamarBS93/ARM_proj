#include "i2cs.h"

#define I2C_SENDER 		(&hi2c4) // Master
#define I2C_RECEIVER 	(&hi2c1) // Slave
#define I2C_SLAVE_ADDR  (0x3C << 1)


result_pro_t i2c_testing(test_command_t* command){


	uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
	uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

	result_pro_t response;
	HAL_StatusTypeDef rx_status, tx_status;

	if (command == NULL) {
        printf("I2C_TEST: Received NULL command pointer. Skipping.\n");
        response.test_result = TEST_ERR;
        return response;
	}

	response.test_id = command->test_id;
	// Copy pattern to TX buffer
    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

	for(uint8_t i=0 ; i< command->iterations ; i++){
	    printf("I2C_TEST: Iteration %u/%u -\n", i + 1, command->iterations);
	    memset(rx_buffer, 0, command->bit_pattern_length);

	    // --- 1. START RECEIVE DMA ---
	    HAL_StatusTypeDef rx_status = HAL_I2C_Slave_Receive_DMA(I2C_RECEIVER, rx_buffer, command->bit_pattern_length);
	    if (rx_status != HAL_OK) {
	        printf("Failed to start receive DMA: %d\n", rx_status);
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        return response;
	    }

	    // --- 2. TRANSMIT a block of data via DMA ---
	    HAL_StatusTypeDef tx_status = HAL_I2C_Master_Transmit_DMA(I2C_SENDER, I2C_SLAVE_ADDR, tx_buffer, command->bit_pattern_length);
	    if (tx_status != HAL_OK) {
	        printf("Failed to send DMA on I2C sender: %d\n", tx_status);
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        HAL_UART_DMAStop(I2C_RECEIVER); // Stop the pending receive
	        return response;
	    }

	    // --- 3. WAIT FOR BOTH TX AND RX DMA COMPLETION ---
	    if (xSemaphoreTake(I2cTxHandle, TIMEOUT) != pdPASS) {
	         printf("fail to get TxSemaphore\n");
	         response.test_result = TEST_FAIL;
	         vPortFree(command);
	         HAL_UART_DMAStop(I2C_RECEIVER); // Stop the pending receive
	         return response;
	    }

	    if (xSemaphoreTake(I2cRxHandle, TIMEOUT) != pdPASS) {
	         printf("fail to get RxSemaphore\n");
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        HAL_UART_DMAStop(I2C_RECEIVER); //Stop the stuck receive
	        return response;
	    }

	    // --- 4. COMPARE SENT vs. RECEIVED data ---
	    if (command->bit_pattern_length > 100) {
			printf("bit_pattern_length more than 100\n");

			// Use CRC comparison for large data
			uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
			uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
			if (sent_crc != received_crc) {
				printf("I2C_TEST: CRC mismatch on iteration %u. Sent CRC: 0x%lX, Received CRC: 0x%lX\n",
					   i + 1, sent_crc, received_crc);
				response.test_result = TEST_FAIL;
				vPortFree(command);
				return response;
			}
	    }
	    else {
			int comp = memcmp(tx_buffer, rx_buffer, command->bit_pattern_length);
			if (comp != 0) {
				printf("Data mismatch on iteration %u.\n", i + 1);
				printf("Sent: %.*s\n", command->bit_pattern_length, tx_buffer);
				printf("Recv: %.*s\n", command->bit_pattern_length, rx_buffer);
				response.test_result = TEST_FAIL;
				vPortFree(command);
				return response;
			}
	    }
	    printf("Data Match on iteration %u.\n", i + 1);

        osDelay(10); // Small delay between iterations to prevent overwhelming the UUT or the system
	}
    response.test_result = TEST_PASS;
    vPortFree(command);
    return response;
}


void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hi2c->Instance == I2C_SENDER->Instance) // Check the instance of your sender UART
    {
        xSemaphoreGiveFromISR(I2cTxHandle, &xHigherPriorityTaskWoken);
        printf("TX callback fired and freed the semaphore\n");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hi2c->Instance == I2C_RECEIVER->Instance) // Check the instance of your receiver UART
    {
        xSemaphoreGiveFromISR(I2cRxHandle, &xHigherPriorityTaskWoken);
        printf("RX callback fired and freed the semaphore\n");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
