#include "spis.h"

/*
 *
 * SPI1 Master (TX DMA)        SPI2 Slave (RX DMA)
 * PA5 SCK   (CN7)  <--------> PB10 SCK (CN10)
 * PA6 MISO	 (CN7)  ---------> PC2 MISO (CN10)
 * PB5 MOSI  (CN7)  <--------- PC3 MOSI (CN9)
 *
 */
#define SPI_SENDER 	    (&hspi1) // Master
#define SPI_RECEIVER	(&hspi2) // Slave

//int g_master_flag = 0;
uint8_t echo_rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
uint8_t echo_tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
/*
 * @brief Performs a test on the SPI peripheral using the command protocol.
 * @param command: A pointer to the test_command_t struct.
 * @retval result_t: The result of the test (TEST_PASS or TEST_FAIL).
 */
Result spi_testing(test_command_t* command){

	static uint8_t tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
	static uint8_t rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
//	static uint8_t echo_rx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};
//	static uint8_t echo_tx_buffer[MAX_BIT_PATTERN_LENGTH] = {0};

	HAL_StatusTypeDef status;

	if (command == NULL) {
        printf("SPI_TEST: Received NULL command pointer. Skipping.\n");
        return TEST_ERR;
	}

    memcpy(tx_buffer, command->bit_pattern, command->bit_pattern_length);

	for(uint8_t i = 0; i < command->iterations; i++)
	{
	    printf("SPI_TEST: Iteration %u/%u -\n\r", i + 1, command->iterations);
	    memset(rx_buffer, 0, command->bit_pattern_length);

	    HAL_SPI_Abort(SPI_SENDER);
	    HAL_SPI_Abort(SPI_RECEIVER);

	    // 1. Prepare Slave for a Receive Operation
	    status = HAL_SPI_TransmitReceive_DMA(SPI_RECEIVER, echo_tx_buffer, echo_rx_buffer, command->bit_pattern_length);
	    if (status != HAL_OK) {
	        printf("Failed to start slave receive: %d\n\r", status);
	        return TEST_FAIL;
	    }

	    // 2. Master Transmits data
	    status = HAL_SPI_TransmitReceive_DMA(SPI_SENDER, tx_buffer, rx_buffer, command->bit_pattern_length);
	    if (status != HAL_OK) {
	        printf("Failed to start master transmit: %d\n\r", status);
	        HAL_SPI_DMAStop(SPI_RECEIVER);
	        return TEST_FAIL;
	    }

	    // 3. Wait for the Master's Transmit to complete
	    if (xSemaphoreTake(SpiTxHandle, TIMEOUT) != pdPASS) {
	         printf("Master TX timeout\n\r");
	         HAL_SPI_Abort(SPI_SENDER);
	         HAL_SPI_Abort(SPI_RECEIVER);
		     return TEST_FAIL;
	    }
	    else
	    {

	    }
	    // 4. Wait for the Slave's Receive to complete, which triggers its echo back
	    if (xSemaphoreTake(SpiSlaveRxHandle, TIMEOUT) != pdPASS) {
	         printf("Slave RX timeout\n\r");
	         HAL_SPI_Abort(SPI_SENDER);
	         HAL_SPI_Abort(SPI_RECEIVER);
	         return TEST_FAIL;
	    }
	    else
	    {
	    	// 5. Now, prepare Master to Receive the Echoed data
		    status = HAL_SPI_Receive_DMA(SPI_SENDER, rx_buffer, command->bit_pattern_length);
		    if (status != HAL_OK) {
		        printf("Failed to start master Rx: %d\n\r", status);
		        HAL_SPI_Abort(SPI_SENDER);
		        HAL_SPI_Abort(SPI_RECEIVER);
		        return TEST_FAIL;
		    }
		    status = HAL_SPI_Transmit_DMA(SPI_RECEIVER, echo_tx_buffer, command->bit_pattern_length);
		    if (status != HAL_OK) {
		        printf("Failed to start slave transmit: %d\n\r", status);
		        HAL_SPI_Abort(SPI_RECEIVER);
		        HAL_SPI_Abort(SPI_SENDER);
		        return TEST_FAIL;
		    }
	    }

	    // 6. Wait for Master's final Receive to complete
	    if (xSemaphoreTake(SpiRxHandle, TIMEOUT) != pdPASS) {
	         printf("Master RX timeout\n\r");
	         HAL_SPI_Abort(SPI_SENDER);
	         HAL_SPI_Abort(SPI_RECEIVER);
	         return TEST_FAIL;
	    }

	    // 7. Compare Sent vs. Received data
	    if (command->bit_pattern_length > 100) {
	        uint32_t sent_crc = calculate_crc(tx_buffer, command->bit_pattern_length);
	        uint32_t received_crc = calculate_crc(rx_buffer, command->bit_pattern_length);
	        if (sent_crc != received_crc) {
	            printf("SPI_TEST: CRC mismatch on iteration %u.\n", i + 1);
	            return TEST_FAIL;
	        }
	    }
	    else
	    {
	        int comp = memcmp(tx_buffer, rx_buffer, command->bit_pattern_length);
	        if (comp != 0) {
	            printf("Data mismatch on iteration %u.\n", i + 1);
				printf("Sent: %.*s\n", command->bit_pattern_length, tx_buffer);
				printf("Recv: %.*s\n", command->bit_pattern_length, rx_buffer);
	            return TEST_FAIL;
	        }
	    }
	    printf("Data Match on iteration %u.\n", i + 1);

        osDelay(10);
	}

    return TEST_PASS;
}

// Tx Complete Callback
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hspi->Instance == SPI_SENDER->Instance)
    {
        xSemaphoreGiveFromISR(SpiTxHandle, &xHigherPriorityTaskWoken);
        printf("Master Tx callback fired\n\r");
    }
    else if(hspi->Instance == SPI_RECEIVER->Instance)
    {
        printf("Slave Tx callback fired\n\r");
    }
    else
    {
    	UNUSED(hspi);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Rx Complete Callback
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hspi->Instance == SPI_RECEIVER->Instance)
    {
        xSemaphoreGiveFromISR(SpiSlaveRxHandle, &xHigherPriorityTaskWoken);
        printf("Slave Rx callback fired, starting echo\n\r");
    }
    else if (hspi->Instance == SPI_SENDER->Instance)
    {
        printf("Master Rx callback fired\n\r");
        xSemaphoreGiveFromISR(SpiRxHandle, &xHigherPriorityTaskWoken);
    }
    else
    {
    	UNUSED(hspi);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hspi->Instance == SPI_RECEIVER->Instance)
    {
        xSemaphoreGiveFromISR(SpiSlaveRxHandle, &xHigherPriorityTaskWoken);
        printf("Slave TxRx callback fired\n\r");
        memcpy(echo_tx_buffer,echo_rx_buffer, SPI_RECEIVER->RxXferSize);
    }
    else if (hspi->Instance == SPI_SENDER->Instance)
    {
        printf("Master TxRx callback fired\n\r");
//        if (!g_master_flag){
//        	g_master_flag++;
        	xSemaphoreGiveFromISR(SpiTxHandle, &xHigherPriorityTaskWoken);
//        }
//        else{
//        	g_master_flag --;
//        	xSemaphoreGiveFromISR(SpiRxHandle, &xHigherPriorityTaskWoken);
//        }
    }
    else
    {
    	UNUSED(hspi);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


