#include "spis.h"

/*
 *
 * SPI1 (master)              SPI2 (slave)
 * PA5 SCK   (CN7)  <--------> PB10 SCK (CN10)
 * PA6 MISO	 (CN7)  <--------- PC2 MISO (CN10)
 * PB5 MOSI  (CN7)  ---------> PC3 MOSI (CN9)
 *
 */
#define SPI_SENDER 		(&hspi1) // Master
#define SPI_RECEIVER 	(&hspi2) // Slave


result_pro_t spi_testing(test_command_t* command){

	uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
	uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

	result_pro_t response;
	HAL_StatusTypeDef rx_status, tx_status;

	if (command == NULL) {
//        printf("SPI_TEST: Received NULL command pointer. Skipping.\n\r"); // Debug printf
        response.test_result = TEST_ERR;
        return response;
	}

	response.test_id = command->test_id;

    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

	for(uint8_t i=0 ; i< command->iterations ; i++){
//	    printf("SPI_TEST: Iteration %u/%u -\n\r", i + 1, command->iterations); // Debug printf
	    memset(rx_buffer, 0, command->bit_pattern_length);

	    // START RECEIVE DMA FIRST (SLAVE)
	    rx_status = HAL_SPI_Receive_IT(SPI_RECEIVER, rx_buffer, command->bit_pattern_length);
	    if (rx_status != HAL_OK) {
//	        printf("Failed to start slave receive DMA: %d\n\r", rx_status); // Debug printf
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        return response;
	    }

	    // TRANSMIT a block of data via DMA (MASTER)
	    tx_status = HAL_SPI_Transmit(SPI_SENDER, tx_buffer, command->bit_pattern_length,TIMEOUT);
	    if (tx_status != HAL_OK) {
//	        printf("Failed to send DMA on SPI sender: %d\n\r", tx_status); // Debug printf
	        response.test_result = TEST_FAIL;
	        vPortFree(command);
	        HAL_SPI_Abort_IT(SPI_SENDER); // Stop the stuck sending
		    HAL_SPI_Abort_IT(SPI_RECEIVER); // Stop the pending receive

	        return response;
	    }

	    // WAIT FOR RX DMA COMPLETION
	    if (xSemaphoreTake(SpiRxHandle, TIMEOUT) != pdPASS) {
//	         printf("Slave RX timeout\n\r"); // Debug printf
	         response.test_result = TEST_FAIL;
	         vPortFree(command);
	         HAL_SPI_Abort_IT(SPI_RECEIVER);
	         return response;
	    }

		// COMPARE SENT vs. RECEIVED data
	    if (command->bit_pattern_length > 100) {
	        uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
	        uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
	        if (sent_crc != received_crc) {
//	            printf("SPI_TEST: CRC mismatch on iteration %u.\n\r", i + 1); // Debug printf
	            response.test_result = TEST_FAIL;
	            vPortFree(command);
	            return response;
	        }
	    } else {
	        int comp = memcmp(tx_buffer, rx_buffer, command->bit_pattern_length);
	        if (comp != 0) {
//	            printf("Data mismatch on iteration %u.\n\r", i + 1); // Debug printf
	            response.test_result = TEST_FAIL;
	            vPortFree(command);
	            return response;
	        }
	    }
//	    printf("Data Match on iteration %u.\n\r", i + 1); // Debug printf

        osDelay(10);
	}
    response.test_result = TEST_PASS;
    vPortFree(command);
    return response;
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hspi->Instance == SPI_RECEIVER->Instance) {
        xSemaphoreGiveFromISR(SpiRxHandle, &xHigherPriorityTaskWoken);
//        printf("Slave Rx callback fired and gave a semaphore\n\r"); // Debug printf
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


