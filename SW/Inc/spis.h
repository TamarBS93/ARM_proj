#ifndef SPIS_H_
#define SPIS_H_

#include <stdint.h>
#include "cmsis_os.h"

#include "FreeRTOS.h"
#include "semphr.h" // For semaphore-specific functions and types like SemaphoreHandle_t

#include "stm32f7xx_hal.h" // General HAL header, often includes peripheral specific ones

#include "project_header.h"

#define TIMEOUT 	1000 	// ticks (60  millis).

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi4;

extern osSemaphoreId_t SpiTxHandle;
extern osSemaphoreId_t SpiRxHandle;
extern osSemaphoreId_t SpiSlaveRxHandle;

Result spi_testing(test_command_t*);
void clear_flags(SPI_HandleTypeDef *hspi);
void reset_test();

#endif /* SPIS_H_ */
