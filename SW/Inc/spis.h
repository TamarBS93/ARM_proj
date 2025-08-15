#ifndef SPIS_H_
#define SPIS_H_

#include <stdint.h>
#include "cmsis_os.h"

#include "FreeRTOS.h"
#include "semphr.h" // For semaphore-specific functions and types like SemaphoreHandle_t

#include "stm32f7xx_hal.h" // General HAL header, often includes peripheral specific ones
#include "stm32f7xx_hal_uart.h" // Specifically for UART_HandleTypeDef and HAL_UART functions

#include "project_header.h"

#define TIMEOUT 	2000 	// ticks (60  millis).

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

extern osSemaphoreId_t SpiTxHandle;
extern osSemaphoreId_t SpiRxHandle;

result_pro_t spi_testing(test_command_t*);

#endif /* SPIS_H_ */
