/* USER CODE BEGIN Header */
/**
******************************************************************************
* @file           : main.c
* @brief          : Main program body
******************************************************************************
* @attention
*
* Copyright (c) 2022 STMicroelectronics.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ExpressLink.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MQTT_PUBLISH_TIME_BETWEEN_MS         ( 3000 )

#define TOPIC_INDEX                          1

#define NUMBER_OF_LOOPS   (3600000/MQTT_PUBLISH_TIME_BETWEEN_MS) /* Loop for 1h */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char * response;

char payloadBuf[ EXPRESSLINK_TX_BUFFER_SIZE ];
char topic     [ EXPRESSLINK_TX_BUFFER_SIZE ];

MQTTPublishInfo_t msg;

int n = 0;

static unsigned long last_send_time = 0;

uint32_t nloops = NUMBER_OF_LOOPS;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

static void appInit(void);
static void Process(void);

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
#if EXPRESSLINK_DEBUG
    printf("[INFO] Message receiced on topic %d\r\n", (int)event.param);
#endif
    
    ExpressLink_GetMessage(&msg);
    
    printf("[INFO] Message received:\r\n Topic  : %s Message: %s\r\n", msg.pTopic, msg.pPayload);
    break;
    
  case EXPRESSLINK_EVENT_STARTUP:
    {
#if EXPRESSLINK_DEBUG      
      printf("[INFO] Module started\r\n");
#endif
      
      ExpressLink_SetState(EXPRESSLINK_STATE_READY);
    }
    break;
    
  case EXPRESSLINK_EVENT_CONLOST:
    {
#if EXPRESSLINK_DEBUG
      printf("[ERROR] Connection lost\r\n");
#endif
      ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);
    }
    break;
    
  case EXPRESSLINK_EVENT_OVERRUN:
    {
#if EXPRESSLINK_DEBUG
      printf("[ERROR] Overrun error\r\n");
#endif
      
      ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);
    }
    break;
    
  case EXPRESSLINK_EVENT_OTA:
#if EXPRESSLINK_DEBUG
    printf("[INFO] OTA received\r\n");
#endif
    break;
    
  case EXPRESSLINK_EVENT_CONNECT:
#if EXPRESSLINK_DEBUG
    printf("[INFO] Connect event with Connection Hint %d\r\n", (int)event.param);
#endif
    break;
    
  case EXPRESSLINK_EVENT_CONFMODE:
#if EXPRESSLINK_DEBUG
    printf("[INFO] Conf mode\r\n");
#endif
    break;
    
  case EXPRESSLINK_EVENT_SUBACK:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A subscription was accepted. Topic Index %d\r\n", (int)event.param);
#endif    
    break;
    
  case EXPRESSLINK_EVENT_SUBNACK:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A subscription was rejected. Topic Index %d\r\n", (int)event.param);
#endif     
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_INIT:
#if EXPRESSLINK_DEBUG
    printf("[INFO] Shadow[Shadow Index] interface was initialized successfully. Topic Index %d\r\n", (int)event.param);
#endif       
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_INIT_FAILED:
#if EXPRESSLINK_DEBUG
    printf("[INFO] The SHADOW[Shadow Index] interface initialization failed. Topic Index %d\r\n", (int)event.param);
#endif 
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_INIT_DOC:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow document was received. Topic Index %d\r\n", (int)event.param);
#endif    
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_UPDATE:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow update result was received. Topic Index %d\r\n", (int)event.param);
#endif     
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_DELTA:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow delta update was received. Topic Index %d\r\n", (int)event.param);
#endif    
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_DELETE:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow delete result was received. Topic Index %d\r\n", (int)event.param);
#endif       
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_SUBACK:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow delta subscription was accepted. Topic Index %d\r\n", (int)event.param);
#endif      
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_SUBNACK:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow delta subscription was rejected. Topic Index %d\r\n", (int)event.param);
#endif       
    break;
    
  default:
#if EXPRESSLINK_DEBUG
    printf("[INFO] Other event received: %d\r\n", (int)event.id);
#endif
    break;
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
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  
  appInit();
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Process();
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
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2)
  {
  }

  /* HSI configuration and activation */
  LL_RCC_HSI_Enable();
  while(LL_RCC_HSI_IsReady() != 1)
  {
  }

  /* Main PLL configuration and activation */
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLR_DIV_2);
  LL_RCC_PLL_Enable();
  LL_RCC_PLL_EnableDomain_SYS();
  while(LL_RCC_PLL_IsReady() != 1)
  {
  }

  /* Set AHB prescaler*/
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

  /* Sysclk activation on the main PLL */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  }

  /* Set APB1 prescaler*/
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  /* Update CMSIS variable (which can be updated also through SystemCoreClockUpdate function) */
  LL_SetSystemCoreClock(64000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : _ExpressLink_EVENT_Pin */
  GPIO_InitStruct.Pin = _ExpressLink_EVENT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(_ExpressLink_EVENT_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
* @brief ExpressLink module Configuration
* @retval None
*/
void appInit(void)
{  
  ExpressLink_event_t event;

  msg.pTopic   = topic;
  msg.pPayload = payloadBuf;
  
  printf("[INFO] Starting\r\n");
  
  /* Detect if a ExpressLink module is present */
    do
    {
      HAL_Delay(ExpressLink_BOOT_DELAY);
      response = ExpressLink_Test();

      if(strstr(response, "OK") == NULL)
      {
    	  printf("[ERRR] Please make sure the ExpressLink module is plugged in and turned on!\r\n");
      }
    }while(strstr(response, "OK") == NULL);

  #if EXPRESSLINK_DEBUG
      printf("[INFO] Resetting ExpressLink module\r\n");
  #endif
    /* Reset the module */
    ExpressLink_Reset();

    do
    {
      event = ExpressLink_GetEvent(); /* Get event  */
     } while ((event.id == EXPRESSLINK_EVENT_ERROR) || (event.id == EXPRESSLINK_EVENT_NONE));

    ExpressLink_SetState(EXPRESSLINK_STATE_READY);

    /* Chack if it is a Wi-Fi or cellular module */
    response = ExpressLink_GetAPN();

    /* If Wi-Fi then set SSID and password */
    if(strstr(response, "OK") == NULL)
    {
  #if EXPRESSLINK_DEBUG
      printf("[INFO] Wi-Fi\r\n");
  #endif
      /* Set Wi-Fi SSID and password */
      ExpressLink_SetSSID      ((char *)&EXPRESSLINK_WIFI_SSID);
      ExpressLink_SetPassphrase((char *)&EXPRESSLINK_WIFI_PASSWORD);
    }
    else
    {
  #if EXPRESSLINK_DEBUG
      printf("[INFO] Cellular\r\n");
  #endif
    }

    ExpressLink_SetCustomName("STM32-EL-MX");
  
  /* Connect to AWS */
  response = ExpressLink_Connect();
  
  if(strcmp(response,"OK 1 CONNECTED\r\n") == 0)
  {
    printf("[INFO] Connected to AWS\r\n");
    
    ExpressLink_SetState(EXPRESSLINK_STATE_CONNECTED);
  }
  else
  {
    printf("[ERROR] %s", response);
    ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);

    NVIC_SystemReset();
  }
  
  /* If we are successfuly connected to AWS, then configure the topics */
  if (ExpressLink_GetState() == EXPRESSLINK_STATE_CONNECTED)
  {
    /* Get the thing name */
    response = ExpressLink_GetThingName();
    response = ExpressLink_SetTopic        (TOPIC_INDEX    , response      );
    response = ExpressLink_SubscribeToTopic(TOPIC_INDEX);
  }
}

static void Process(void)
{
  ExpressLink_EventCallback(ExpressLink_GetEvent());
  
  if (ExpressLink_GetState() != EXPRESSLINK_STATE_CONNECTED)
  {
    NVIC_SystemReset();
  }
  
  if (HAL_GetTick() - last_send_time >= MQTT_PUBLISH_TIME_BETWEEN_MS)
  {
    last_send_time = HAL_GetTick();
    
    snprintf( payloadBuf, EXPRESSLINK_TX_BUFFER_SIZE, "Hello world %d!", n++);
    
    printf("[INFO] Sending MQTT message: %s\r\n", payloadBuf);
    
    response = ExpressLink_SendMessage(TOPIC_INDEX, payloadBuf);
    
      if (nloops-- == 0)
      {
        ExpressLink_Disonnect();
        
        printf("The demo has ended\r\n");

        __disable_irq();

        while(1)
        {
          ;
        }
      }
  }
}
/* USER CODE END 4 */

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
