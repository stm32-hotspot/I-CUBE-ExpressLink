/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include "ExpressLink.h"
#include "metadata.h"

#if DEMO_SHADOW
#include "shadow_device_task.h"
#endif

#if DEMO_PUB_SUB
#include <PubSub.h>
#endif

#if DEMO_SENSORS_ENV
#include <env_sensor_publish.h>
#endif

#if DEMO_SENSORS_MOTION
#include <motion_sensor_publish.h>
#endif

#if DEMO_OTA
#include "ota.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 0: no debug, 1: Errors only, 2: INFO, 3: all (Error, Info, Debug)  */
#define DEBUG_LEVEL 2

#include "debug.h"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_NodeTypeDef Node_GPDMA1_Channel0;
DMA_QListTypeDef List_GPDMA1_Channel0;
DMA_HandleTypeDef handle_GPDMA1_Channel0;
DMA_NodeTypeDef Node_GPDMA1_Channel1;
DMA_QListTypeDef List_GPDMA1_Channel1;
DMA_HandleTypeDef handle_GPDMA1_Channel1;

/* USER CODE BEGIN PV */
SemaphoreHandle_t ExpressLink_MutexHandle = NULL;
SemaphoreHandle_t SENSORS_I2C_MutexHandle = NULL;

EventGroupHandle_t ExpressLink_EventHandle = NULL;

TaskHandle_t defaultTaskHandle = NULL;
TaskHandle_t ExpressLinkTask_Handle = NULL;
TaskHandle_t ConfigTask_Handle = NULL;

#if DEMO_SHADOW
TaskHandle_t ShadowDemoTask_Handle = NULL;
#endif

#if DEMO_SENSORS_ENV
TaskHandle_t EnvSensorPublishTask_Handle = NULL;
#endif

#if DEMO_SENSORS_MOTION
TaskHandle_t MotionSensorPublishTask_Handle = NULL;
#endif

#if DEMO_PUB_SUB
TaskHandle_t PubSubTask_Handle = NULL;
#endif

#if DEMO_OTA
TaskHandle_t OTATask_Handle = NULL;
#endif

metadata_t Metadata;
uint8_t console_data = 0;
uint8_t config_mode = 0;

uint32_t starting = 1;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_GPDMA1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ICACHE_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
void vMainTask(void *pvParameters);

void vExpressLinkTask(void *pvParameters);

void vConfigTask(void *argument);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void ExpressLink_EventCallback(ExpressLink_event_t event)
{
  switch (event.id)
  {
  case EXPRESSLINK_EVENT_NONE:
    break;

  case EXPRESSLINK_EVENT_MESSAGE:
    PRINTF_DEBUG("[EVENT] Message received\r\n");

    xEventGroupSetBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_MESSAGE);
    break;

  case EXPRESSLINK_EVENT_STARTUP:
    PRINTF_DEBUG("[EVENT] Module started\r\n");

    ExpressLink_SetState(EXPRESSLINK_STATE_READY);
    xEventGroupSetBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_STARTUP);
    break;

  case EXPRESSLINK_EVENT_CONLOST:
    PRINTF_DEBUG("[EVENT] Connection lost\r\n");

    ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);
    break;

  case EXPRESSLINK_EVENT_OVERRUN:
    PRINTF_DEBUG("[EVENT] Overrun error\r\n");

    ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);
    break;

  case EXPRESSLINK_EVENT_OTA:
    PRINTF_DEBUG("[EVENT] OTA Event received\r\n");

    xEventGroupSetBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_OTA);

    break;

  case EXPRESSLINK_EVENT_CONNECT:
    PRINTF_DEBUG("[EVENT] Connect event with Connection Hint %d\r\n", (int) event.param);

    if (event.param == 0)
    {
      ExpressLink_SetState(EXPRESSLINK_STATE_CONNECTED);

      xEventGroupSetBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_CONNECT);
    }
    else
    {
      ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);
    }
    break;

  case EXPRESSLINK_EVENT_CONFMODE:
    PRINTF_DEBUG("[EVENT] Conf mode\r\n");
    break;

  case EXPRESSLINK_EVENT_SUBACK:
    PRINTF_DEBUG("[EVENT] A subscription was accepted. Topic Index %d\r\n", (int) event.param);

    xEventGroupSetBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_SUBACK);
    break;

  case EXPRESSLINK_EVENT_SUBNACK:
    PRINTF_DEBUG("[EVENT] A subscription was rejected. Topic Index %d\r\n", (int) event.param);
    break;

  case EXPRESSLINK_EVENT_SHADOW_INIT:
  case EXPRESSLINK_EVENT_SHADOW_INIT_FAILED:
  case EXPRESSLINK_EVENT_SHADOW_INIT_DOC:
  case EXPRESSLINK_EVENT_SHADOW_UPDATE:
  case EXPRESSLINK_EVENT_SHADOW_DELTA:
  case EXPRESSLINK_EVENT_SHADOW_DELETE:
  case EXPRESSLINK_EVENT_SHADOW_SUBACK:
  case EXPRESSLINK_EVENT_SHADOW_SUBNACK:
#if DEMO_SHADOW
    if (event.param == 0)/* Unnamed Shadow */
    {
      xTaskNotifyIndexed(ShadowDemoTask_Handle, 0, event.id, eSetBits);
    }
#endif
    break;

  default:
    PRINTF_DEBUG("[EVENT] Other event received: %d\r\n", (int) event.id);
    break;
  }
}

#if defined(ExpressLink_EVENT_Pin)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == ExpressLink_EVENT_Pin)
  {
    BaseType_t xHigherPriorityTaskWoken;

    xHigherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveIndexedFromISR(ExpressLinkTask_Handle, 0, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == ExpressLink_EVENT_Pin)
  {
    BaseType_t xHigherPriorityTaskWoken;

    xHigherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveIndexedFromISR(ExpressLinkTask_Handle, 0, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}
#endif

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == CONSOLE_UART.Instance)
  {
    do
    {
      BaseType_t xHigherPriorityTaskWoken;

      xHigherPriorityTaskWoken = pdFALSE;

      vTaskNotifyGiveIndexedFromISR(ConfigTask_Handle, 0, &xHigherPriorityTaskWoken);

      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    while (0);
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_GPDMA1_Init();
  MX_USART1_UART_Init();
  MX_ICACHE_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  ExpressLink_MutexHandle = xSemaphoreCreateMutex();
  SENSORS_I2C_MutexHandle = xSemaphoreCreateMutex();

  ExpressLink_EventHandle = xEventGroupCreate();

  xTaskCreate(vMainTask, "MainTask", configMINIMAL_STACK_SIZE, (void*) 1, PRIORITY_MainTask, &defaultTaskHandle);
#if defined(ExpressLink_EVENT_Pin)
  xTaskCreate(vExpressLinkTask, "ExpressLinkTask", configMINIMAL_STACK_SIZE, (void*) 1, PRIORITY_ExpressLinkTask, &ExpressLinkTask_Handle);
#endif
  xTaskCreate(vConfigTask, "ConfigTask", configMINIMAL_STACK_SIZE, (void*) 1, PRIORITY_MainTask, &ConfigTask_Handle);

  /* Start the RTOS scheduler, this function should not return as it causes the
   execution context to change from main() to one of the created tasks. */
  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 31;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 2048;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPDMA1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPDMA1_Init(void)
{

  /* USER CODE BEGIN GPDMA1_Init 0 */

  /* USER CODE END GPDMA1_Init 0 */

  /* Peripheral clock enable */
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  /* USER CODE BEGIN GPDMA1_Init 1 */

  /* USER CODE END GPDMA1_Init 1 */
  /* USER CODE BEGIN GPDMA1_Init 2 */

  /* USER CODE END GPDMA1_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : _ExpressLink_EVENT_Pin */
  GPIO_InitStruct.Pin = _ExpressLink_EVENT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(_ExpressLink_EVENT_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
#if defined(ExpressLink_EVENT_Pin)
/* Task that handles ExpressLink events. */
void vExpressLinkTask(void *argument)
{
  /* USER CODE BEGIN vExpressLinkTask */
  ExpressLink_event_t event;

  /* Infinite loop */
  for (;;)
  {
    ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);

    /* Give time to the module to boot after power on. The module will not respond if it did not boot yet */
    if(starting)
    {
    	starting = 0;
    	vTaskDelay(pdMS_TO_TICKS(ExpressLink_BOOT_DELAY));
    }

    do
    {
      xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
      taskENTER_CRITICAL();
      event = ExpressLink_GetEvent();
      ExpressLink_EventCallback(event);

      if (ExpressLink_GetState() == EXPRESSLINK_STATE_ERROR)
      {
        PRINTF_ERROR("[ERROR] vExpressLinkTask Generating System reset\r\n");
         vTaskDelay(pdMS_TO_TICKS(2000));
        /* Something went wrong. Generate a system reset */
        NVIC_SystemReset();
      }

      taskEXIT_CRITICAL();
      xSemaphoreGive(ExpressLink_MutexHandle);
    }while (EXPRESSLINK_EVENT_NONE != event.id);
  }
  /* USER CODE END vExpressLinkTask */
}
#else
/* Task that handles ExpressLink events. */
void vExpressLinkTask(void *argument)
{
  /* USER CODE BEGIN vExpressLinkTask */
  ExpressLink_event_t event;

  /* Infinite loop */
  for (;;)
  {
    xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
    taskENTER_CRITICAL();
    event = ExpressLink_GetEvent();
    ExpressLink_EventCallback(event);

    if (ExpressLink_GetState() == EXPRESSLINK_STATE_ERROR)
    {
      PRINTF_ERROR("[ERROR] vExpressLinkTask Generating System reset\r\n");
      vTaskDelay(pdMS_TO_TICKS(2000));
      /* Something went wrong. Generate a system reset */
      NVIC_SystemReset();
    }

    taskEXIT_CRITICAL();
    xSemaphoreGive(ExpressLink_MutexHandle);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  /* USER CODE END vExpressLinkTask */
}
#endif
/* Main task. */
void vMainTask(void *pvParameters)
{
  PRINTF_INFO("[INFO] Starting\r\n");

  metadata_init(&Metadata);
  HAL_UART_Receive_IT(&CONSOLE_UART, &console_data, 1);

#if defined(ExpressLink_EVENT_Pin)
  EventBits_t flag;

  PRINTF_INFO("[INFO] Interrupt mode\r\n");
  /* Delay for Module boot time */
  flag = xEventGroupWaitBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_STARTUP, pdTRUE, pdTRUE, pdMS_TO_TICKS(ExpressLink_BOOT_DELAY));

  if (flag != EXPRESSLINK_EVENT_STARTUP)
  {
    /* Reset the ExpressLink module */
    xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
    ExpressLink_Reset();
    xSemaphoreGive(ExpressLink_MutexHandle);

    flag = xEventGroupWaitBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_STARTUP, pdTRUE, pdTRUE, pdMS_TO_TICKS(ExpressLink_BOOT_DELAY));
  }
#else
  ExpressLink_event_t event;
  char *result;

  PRINTF_INFO("[INFO] Polling mode\r\n");
  PRINTF_INFO("[INFO] Detecting ExpressLink module\r\n");

  do
  {
    vTaskDelay(pdMS_TO_TICKS(ExpressLink_BOOT_DELAY));

    result = ExpressLink_Test();

    if (strstr(result, "OK") == NULL)
    {
      printf("[ERRR] Please make sure the ExpressLink module is plugged in and turned on!\r\n");
    }

  }
  while (strstr(result, "OK") == NULL);

  PRINTF_INFO("[INFO] Resetting the module\r\n");

  ExpressLink_Reset();

  PRINTF_INFO("[INFO] Waiting for the module to restart\r\n");

  do
  {
    event = ExpressLink_GetEvent(); /* Get event  */
  }
  while (event.id == EXPRESSLINK_EVENT_ERROR);

  ExpressLink_SetState(EXPRESSLINK_STATE_READY);
#endif

  xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
  /* Check the module tech spec */
  if (strcmp(ExpressLink_GetTechSpec(), ExpressLink_TechSpec) < 0)
  {
    xSemaphoreGive(ExpressLink_MutexHandle);
    printf("[ERROR] Module TechSpec %s", ExpressLink_GetTechSpec());
    PRINTF_INFO("[INFO] Supported TechSpec %s\r\n", ExpressLink_TechSpec);
    PRINTF_INFO("[INFO] Please update module firmware\r\n");

    vTaskSuspend( NULL);
  }

  PRINTF_INFO("[INFO] Module         : %s", ExpressLink_GetAbout());
  PRINTF_INFO("[INFO] Module TechSpec: %s", ExpressLink_GetTechSpec());
  PRINTF_INFO("[INFO] Module Firmware: %s", ExpressLink_GetVersion());

  xSemaphoreGive(ExpressLink_MutexHandle);

  /* Needed by the the QC script to know that is OK to communicate with the device */
  PRINTF("STM32 OK\r\n");

  /* Give time to Quick Connect script to enter config mode */
  vTaskDelay(pdMS_TO_TICKS(2000));

  PRINTF_INFO("[INFO] Initializing the ExpressLink module\r\n");

  xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);

  if (strstr(ExpressLink_GetSSID(), "OK") != NULL) /* Is this a Wi-Fi module? */
  {
    PRINTF_INFO("[INFO] Wi-Fi\r\n");
    ExpressLink_SetSSID(Metadata.pWIFI_SSID); /* Set Wi-Fi SSID     */
    ExpressLink_SetPassphrase(Metadata.pWIFI_PASSWORD); /* Set Wi-Fi password */
  }
  else
  {
    PRINTF_INFO("[INFO] Cellular\r\n");
    ExpressLink_SetAPN(Metadata.pAPN); /* Set APN */
  }

  ExpressLink_SetCustomName("STM32-EL-MX");

  ExpressLink_SetEndpoint(Metadata.pENDPOINT);/* Set AWS IoT Core endpoint, Wi-Fi SSID and password */
  ExpressLink_SetDefenderPeriod(60 * 10); /* Set the Device Defender upload period              */

  PRINTF_INFO("[INFO] Connecting to AWS\r\n");

  /* Connect to AWS */
  ExpressLink_ConnectAsync();

  xSemaphoreGive(ExpressLink_MutexHandle);

#if !defined(ExpressLink_EVENT_Pin)
  xTaskCreate(vExpressLinkTask, "ExpressLinkTask", configMINIMAL_STACK_SIZE, (void*) 1, PRIORITY_ExpressLinkTask, &ExpressLinkTask_Handle);
#endif

  xEventGroupWaitBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_CONNECT, pdTRUE, pdTRUE, portMAX_DELAY);

  PRINTF_INFO("[INFO] Connected to AWS\r\n");

#if DEMO_OTA
  xTaskCreate(vOTATask, "OTATask", configMINIMAL_STACK_SIZE * 2, (void*) 1, PRIORITY_OTATask, &OTATask_Handle);
#endif

#if DEMO_SHADOW
  xTaskCreate(vShadowDemoTask, "ShadowDemoTask", configMINIMAL_STACK_SIZE, (void*) 1, PRIORITY_ShadowDemoTask, &ShadowDemoTask_Handle);
#endif

#if DEMO_SENSORS_ENV
  xTaskCreate(vEnvSensorPublishTask, "EnvSensorPublishTask", configMINIMAL_STACK_SIZE, (void*) 1, PRIORITY_EnvSensorPublishTask, &EnvSensorPublishTask_Handle);
#endif

#if DEMO_SENSORS_MOTION
  xTaskCreate(vMotionSensorPublishTask, "MotionSensorPublishTask", configMINIMAL_STACK_SIZE, (void*) 1, PRIORITY_MotionSensorPublishTask, &MotionSensorPublishTask_Handle);
#endif

#if DEMO_PUB_SUB
  xTaskCreate(vPubSubTask, "PubSubTask", configMINIMAL_STACK_SIZE, (void*) 1, PRIORITY_PubSubTask, &PubSubTask_Handle);
#endif

  vTaskDelete( NULL);

  /*for (;;)
   {
   vTaskSuspend( NULL);
   }*/
}

void vConfigTask(void *argument)
{
  /* USER CODE BEGIN vExpressLinkTask */
  setvbuf(stdin, NULL, _IONBF, 0);

  /* Infinite loop */
  for (;;)
  {
    ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);

    do
    {
      static uint32_t n = 0; /* Number of received characters */
      static uint8_t data[6];/* Received data */

      ExpressLink_state_t state = ExpressLink_GetState();

      data[n] = tolower((char )console_data);

      n++;

      /* Need to make sure that we can communicate with the module */
      if (!((state == EXPRESSLINK_STATE_CONNECTED) || (state == EXPRESSLINK_STATE_READY)))
      {
        memset(data, 0, 5);
        n = 0;
        printf("NACK\r\n");
        HAL_UART_Receive_IT(&CONSOLE_UART, &console_data, 1);
        continue;
      }

      /* We can communicate with the module */
      /* Read 5 characters */
      if (n < 5)
      {
        HAL_UART_Receive_IT(&CONSOLE_UART, &console_data, 1);
        break;
      }

      /* Check if we received "stm32" */
      if (strcmp((char*) data, "stm32") != 0)
      {
        memset(data, 0, 5);
        n = 0;
        printf("ERR\r\n");
        HAL_UART_Receive_IT(&CONSOLE_UART, &console_data, 1);

        break;
      }

      /* We received "stm32". Enter config mode */
      memset(data, 0, 5);
      n = 0;
      config_mode = 1;
      printf("ACK\r\n");

      xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
      taskENTER_CRITICAL();

      while (1)
      {
        metadata_process_command(metadata_display_menu());
      }
    }
    while (0);
  }
  /* USER CODE END vExpressLinkTask */
}

void vApplicationIdleHook(void)
{
  HAL_Delay(0x100000);
}

void vApplicationMallocFailedHook(void)
{
  PRINTF_ERROR("[ERROR] Failed to allocate memory!\r\n");

  Error_Handler();
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();

  while (1)
  {
    __BKPT(0);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
  ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
