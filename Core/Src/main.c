/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * 14.02.25 by Kurein M.N.
 *
 * v.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
/*#include "../Inc/main.h"*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "DataFile.h"
#include "FreeRTOS.h"
#include "crc16.h"
#include "fram.h"
#include "queue.h"
#include "stm32f4xx_hal.h"
#include "task.h"
#include "uart.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

volatile uint32_t last_rx_time; // Time of last received byte
extern volatile char uart1_rx_buf[UART_BUF_SIZE];
fram_t fram;
fram_cfg_t cfgFRAM;
QueueHandle_t uartQueue;

osThreadId_t LedTaskHandle, Uart1TaskHandle;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart6;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .stack_size = 2048,
    .priority = (osPriority_t)osPriorityNormal,
};
/* USER CODE BEGIN PV */
const osThreadAttr_t LedTask_attributes = {
    .name = "LedTask",
    .stack_size = 512,
    .priority = (osPriority_t)osPriorityNormal,
};

const osThreadAttr_t Uart1Task_attributes = {
    .name = "Uart1Task",
    .stack_size = 2048,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART6_UART_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void LedTask(void *argument);
void Uart1Task(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void LedTask(void *pvParameters) {
  while (1) {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void Uart1Task(void *pvParameters) {
  uint8_t command[UART_BUF_SIZE];
  uint8_t ok[2] = {ID_BU, 0x00};
  uint8_t *ok_ptr;
  uint8_t index = 0;
  uint16_t crc = 0;
  char receivedChar;
  char wr_data[7] = {'K', 'U', 'R', 'E', 'I', 'N'};
  char rd_data[7] = {0};
  uart1_put_ch('S'); // Сигнал о запуске задачи

  while (1) {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_15);

    // Waiting for a byte from the queue
    if (xQueueReceive(uartQueue, &receivedChar, pdMS_TO_TICKS(2)) == pdPASS) {
      if (index < UART_BUF_SIZE) {
        command[index++] = receivedChar;
      }
    }

    uint32_t current_time = xTaskGetTickCount();
    uint32_t diff = current_time - last_rx_time;

    if (index > 0 && diff > pdMS_TO_TICKS(UART_TIMEOUT_MS)) {
      crc = process_crc(command, index, true);

      if (crc == 1) {
        switch (command[0]) {
        case 0x70:
          if (command[1] == 0) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
          }
          if (command[1] == 1) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
          }
          break;

        case 2:
          if (command[1] == 0) {
            fram_write(&fram, 0x0150, (uint8_t *)wr_data, sizeof(wr_data));
            HAL_Delay(1000);
            fram_read(&fram, 0x0150, (uint8_t *)rd_data, sizeof(wr_data));
          }
          break;
        }
      }
      ok_ptr = calculate_crc_for_2_bytes(ok);
      for (int i = 0; i < 4; i++) {
        uart1_put_ch(ok_ptr[i]);
      }
      ok_ptr = calculate_crc_for_2_bytes(ok);
      for (int i = 0; i < 4; i++) {
        uart6_put_ch(ok_ptr[i]);
      }

      while (uxQueueMessagesWaiting(uartQueue) > 0) {
        char discardChar;
        xQueueReceive(uartQueue, &discardChar, 0);
      }
      index = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(3));
  }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */
  last_rx_time = 0;

  uint32_t reset_cause = RCC->CSR;
  RCC->CSR |= RCC_CSR_RMVF;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  RCC->CSR |= RCC_CSR_RMVF;
  log_printf("Rst:0x%08lX\r\n", reset_cause);
  HAL_Delay(1000);
  log_printf("Main started, time: %lu ms\r\n", HAL_GetTick());

  fram_cfg_setup(&cfgFRAM);
  fram_init(&fram, &cfgFRAM);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  uartQueue = xQueueCreate(40, sizeof(char));
  if (uartQueue == NULL) {
    Error_Handler(); // Ошибка создания очереди
  }
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle =
      osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  LedTaskHandle = osThreadNew(LedTask, NULL, &LedTask_attributes);

  Uart1TaskHandle = osThreadNew(Uart1Task, NULL, &Uart1Task_attributes);
  if (Uart1TaskHandle == NULL) {
    uart1_put_ch('E');
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */

  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 38400;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* Enable UART interrupts for receiving data */
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart1_rx_buf[0], 1);

  /* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART6_UART_Init(void) {

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13 | GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
  /* USER CODE BEGIN 5 */
  printf_uart3("Default Task running...\r\n");
  uint32_t diag_divider = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    printf_uart3("Tick:%lu\r\n", HAL_GetTick());

    if (++diag_divider >= 5) {
      diag_divider = 0;

      size_t free_heap = xPortGetFreeHeapSize();
      size_t min_heap = xPortGetMinimumEverFreeHeapSize();
      size_t total_heap = (size_t)configTOTAL_HEAP_SIZE;
      UBaseType_t dflt_stack =
          uxTaskGetStackHighWaterMark((TaskHandle_t)defaultTaskHandle);
      UBaseType_t led_stack =
          uxTaskGetStackHighWaterMark((TaskHandle_t)LedTaskHandle);
      UBaseType_t uart_stack =
          uxTaskGetStackHighWaterMark((TaskHandle_t)Uart1TaskHandle);
      size_t dflt_stack_b = ((size_t)dflt_stack) * sizeof(StackType_t);
      size_t led_stack_b = ((size_t)led_stack) * sizeof(StackType_t);
      size_t uart_stack_b = ((size_t)uart_stack) * sizeof(StackType_t);
      size_t dflt_total_b = (size_t)defaultTask_attributes.stack_size;
      size_t led_total_b = (size_t)LedTask_attributes.stack_size;
      size_t uart_total_b = (size_t)Uart1Task_attributes.stack_size;
      unsigned int heap_used_pct =
          (total_heap > 0U)
              ? (unsigned int)(((total_heap - free_heap) * 100U) / total_heap)
              : 0U;
      unsigned int heap_peak_used_pct =
          (total_heap > 0U)
              ? (unsigned int)(((total_heap - min_heap) * 100U) / total_heap)
              : 0U;
      unsigned int dflt_used_pct =
          (dflt_total_b > 0U)
              ? (unsigned int)(((dflt_total_b - dflt_stack_b) * 100U) /
                               dflt_total_b)
              : 0U;
      unsigned int led_used_pct =
          (led_total_b > 0U)
              ? (unsigned int)(((led_total_b - led_stack_b) * 100U) /
                               led_total_b)
              : 0U;
      unsigned int uart_used_pct =
          (uart_total_b > 0U)
              ? (unsigned int)(((uart_total_b - uart_stack_b) * 100U) /
                               uart_total_b)
              : 0U;

      printf_uart3("HeapB F:%u Min:%u Tot:%u\r\n", (unsigned int)free_heap,
                   (unsigned int)min_heap, (unsigned int)total_heap);
      printf_uart3("HeapU now:%u%% peak:%u%%\r\n", heap_used_pct,
                   heap_peak_used_pct);
      printf_uart3("StkB free D:%u L:%u U:%u\r\n", (unsigned int)dflt_stack_b,
                   (unsigned int)led_stack_b, (unsigned int)uart_stack_b);
      printf_uart3("StkU %% D:%u L:%u U:%u\r\n", dflt_used_pct, led_used_pct,
                   uart_used_pct);
    }
  }

  /* USER CODE END 5 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM1 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
