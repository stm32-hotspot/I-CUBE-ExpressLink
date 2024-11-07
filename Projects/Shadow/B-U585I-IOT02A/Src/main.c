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
#include <string.h>
#include <stdlib.h>
#include "ExpressLink.h"
#include "core_json.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct MQTTAgentCommandContext
{
  uint32_t ulCurrentPowerOnState; /* The device current power on state. */
  uint32_t ulReportedPowerOnState;/* The last reported state. It is initialized to an invalid value so that an update is initially sent. */
} ShadowDeviceCtx_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MQTT_PUBLISH_MAX_LEN                 ( EXPRESSLINK_TX_BUFFER_SIZE )
#define MQTT_PUBLICH_TOPIC_STR_LEN           ( EXPRESSLINK_TX_BUFFER_SIZE )

#define shadowexampleSHADOW_REPORTED_JSON \
    "{"                                   \
    "\"state\":{"                         \
    "\"reported\":{"                      \
    "\"powerOn\": \"%1u\""                \
    "}"                                   \
    "}"                                  \
    "}"

#define shadowexampleSHADOW_REPORTED_JSON_LENGTH       ( sizeof( shadowexampleSHADOW_REPORTED_JSON ) - 2 )

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
static char * response;

char ReceiveTopic        [MQTT_PUBLICH_TOPIC_STR_LEN] = { 0 };
char Message             [MQTT_PUBLISH_MAX_LEN      ] = { 0 };

MQTTPublishInfo_t pxPublishInfo = { 0 };
ShadowDeviceCtx_t xShadowCtx    = { 0 };

uint8_t shadowReceived   = 0;
uint8_t console_data     = 0 ;
uint8_t config_mode      = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_ICACHE_Init(void);
/* USER CODE BEGIN PFP */

static void appInit(void);

static void Handle_DeltaCallback(ShadowDeviceCtx_t * pxCtx, MQTTPublishInfo_t * pxPublishInfo );

static void Publish_ShadowUpdate     ( void );

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
      ExpressLink_GetMessage(&pxPublishInfo);
      
      shadowReceived = 1;
      break;
      
    case EXPRESSLINK_EVENT_STARTUP:
#if EXPRESSLINK_DEBUG
      printf("[INFO] Module started\r\n");
#endif
      ExpressLink_SetState(EXPRESSLINK_STATE_READY);
      break;
      
    case EXPRESSLINK_EVENT_CONLOST:
#if EXPRESSLINK_DEBUG
      printf("[ERROR] Connection lost\r\n");
#endif
      ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);
      break;
      
    case EXPRESSLINK_EVENT_OVERRUN:
#if EXPRESSLINK_DEBUG
      printf("[ERROR] Overrun error\r\n");
#endif
      ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);
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
    printf("[INFO] Shadow %d interface was initialized successfully.\r\n", (int)event.param);
#endif
    response = ExpressLink_ShadowDoc(event.param);
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_INIT_FAILED:
#if EXPRESSLINK_DEBUG
    printf("[INFO] The SHADOW %d interface initialization failed.\r\n", (int)event.param);
#endif
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_INIT_DOC:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow %d document was received.\r\n", (int)event.param);
#endif
    response = ExpressLink_ShadowGetDoc(event.param);
    
    snprintf(pxPublishInfo.pPayload, MQTT_PUBLISH_MAX_LEN - 4, "%s", (char *)&response[4]);
    
    pxPublishInfo.payloadLength = strlen(response) - 4;
    
    shadowReceived = 1;
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_UPDATE:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow %d update result was received.\r\n", (int)event.param);
#endif

    ExpressLink_ShadowGetDoc(event.param);
    ExpressLink_GetMessage(&pxPublishInfo);
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_DELTA:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow %d delta update was received.\r\n", (int)event.param);
#endif

    ExpressLink_ShadowDoc(event.param);
    ExpressLink_GetMessage(&pxPublishInfo);
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_DELETE:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow %d delete result was received.\r\n", (int)event.param);
#endif
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_SUBACK:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow %d delta subscription was accepted.\r\n", (int)event.param);
#endif
    break;
    
  case EXPRESSLINK_EVENT_SHADOW_SUBNACK:
#if EXPRESSLINK_DEBUG
    printf("[INFO] A Shadow %d delta subscription was rejected.\r\n", (int)event.param);
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

  /* Enable PWR clock interface */

  LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_PWR);

  /* USER CODE BEGIN Init */
  
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the System Power */
  SystemPower_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_ICACHE_Init();
  /* USER CODE BEGIN 2 */
  appInit();
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    ExpressLink_EventCallback(ExpressLink_GetEvent());

    if (ExpressLink_GetState() != EXPRESSLINK_STATE_CONNECTED)
    {
      NVIC_SystemReset();
    }

    /* Handle received delta shadow message */
    Handle_DeltaCallback(&xShadowCtx, &pxPublishInfo);
      
    /* Check if we need to send a shadow update */
    Publish_ShadowUpdate();

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
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_4)
  {
  }

  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  while (LL_PWR_IsActiveFlag_VOS() == 0)
  {
  }
  LL_RCC_MSIS_Enable();

   /* Wait till MSIS is ready */
  while(LL_RCC_MSIS_IsReady() != 1)
  {
  }

  LL_RCC_MSI_EnableRangeSelection();
  LL_RCC_MSIS_SetRange(LL_RCC_MSISRANGE_0);
  LL_RCC_MSI_SetCalibTrimming(16, LL_RCC_MSI_OSCILLATOR_0);
  LL_RCC_PLL1_ConfigDomain_SYS(LL_RCC_PLL1SOURCE_MSIS, 3, 10, 1);
  LL_RCC_PLL1_EnableDomain_SYS();
  LL_RCC_SetPll1EPodPrescaler(LL_RCC_PLL1MBOOST_DIV_4);
  LL_RCC_PLL1_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_8_16);
  LL_RCC_PLL1_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL1_IsReady() != 1)
  {
  }

   /* Intermediate AHB prescaler 2 when target frequency clock is higher than 80 MHz */
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_2);

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL1)
  {
  }

  /* Insure 1us transition state at intermediate medium speed clock*/
  for (__IO uint32_t i = (160 >> 1); i !=0; i--);

  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_RCC_SetAPB3Prescaler(LL_RCC_APB3_DIV_1);
  LL_SetSystemCoreClock(160000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{

  /*
   * Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
   */
  LL_PWR_DisableUCPDDeadBattery();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  LL_PWR_SetRegulatorSupply(LL_PWR_SMPS_SUPPLY);

  while(LL_PWR_IsActiveFlag_REGULATOR()!=1)
  {
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
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
  LL_ICACHE_SetMode(LL_ICACHE_1WAY);
  LL_ICACHE_Enable();
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
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOD);
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOE);

  /**/
  LL_GPIO_SetOutputPin(LD2_GPIO_Port, LD2_Pin);

  /**/
  GPIO_InitStruct.Pin = ExpressLink_EVENT_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(ExpressLink_EVENT_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

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
  uint32_t clientToken = 1234;
  
  xShadowCtx.ulCurrentPowerOnState  = HAL_GPIO_ReadPin(LD2_GPIO_Port, LD2_Pin);
  xShadowCtx.ulReportedPowerOnState = !xShadowCtx.ulCurrentPowerOnState;
  
  pxPublishInfo.pTopic   = ReceiveTopic;
  pxPublishInfo.pPayload = Message;
  
#if EXPRESSLINK_DEBUG
  printf("[INFO] Starting\r\n");
#endif
  HAL_Delay(ExpressLink_BOOT_DELAY);
  
  /* Reset the ExpressLink module */
  ExpressLink_Reset();
  
  HAL_Delay(ExpressLink_BOOT_DELAY);

  /* Wait until the module is ready */
  do
  {
    ExpressLink_EventCallback(ExpressLink_GetEvent());
  }while(ExpressLink_GetState() != EXPRESSLINK_STATE_READY);
  
  /* Set AWS IoT Core endpoint */
  ExpressLink_SetEndpoint  (EXPRESSLINK_AWS_IOT_ENDPOINT);
  
  /* Set Wi-Fi SSID and password */
  ExpressLink_SetSSID      (EXPRESSLINK_WIFI_SSID);
  ExpressLink_SetPassphrase(EXPRESSLINK_WIFI_PASSWORD);
  
  /* Connect to AWS */
  response = ExpressLink_Connect();

  if(strcmp(response,"OK 1 CONNECTED\r\n") != 0)
  {
#if EXPRESSLINK_DEBUG
    printf("[ERROR] %s", response);
#endif

    ExpressLink_SetState(EXPRESSLINK_STATE_ERROR);

    NVIC_SystemReset();
  }

#if EXPRESSLINK_DEBUG
  printf("[INFO] Connected to AWS\r\n");
#endif
  
  ExpressLink_SetState(EXPRESSLINK_STATE_CONNECTED);
  
  ExpressLink_ShadowEnable();
  ExpressLink_ShadowSetToken(clientToken);
  ExpressLink_ShadowInit(0);
  ExpressLink_ShadowSubscribe(0);
}

static void Publish_ShadowUpdate     ( void )
{
  if(xShadowCtx.ulCurrentPowerOnState != xShadowCtx.ulReportedPowerOnState)
  {
    char pcUpdateDocument[ shadowexampleSHADOW_REPORTED_JSON_LENGTH + 1 ] = { 0 };

    /* Send shadow update */
#if EXPRESSLINK_DEBUG
    printf("[INFO] Updating Shadow\r\n");
#endif
    xShadowCtx.ulReportedPowerOnState = xShadowCtx.ulCurrentPowerOnState;
    
    /* Build the JSON message */
    snprintf( pcUpdateDocument,
             shadowexampleSHADOW_REPORTED_JSON_LENGTH + 1,
             shadowexampleSHADOW_REPORTED_JSON,
             ( unsigned int ) xShadowCtx.ulCurrentPowerOnState );
    
    ExpressLink_ShadowUpdate(0, pcUpdateDocument);
  }
}

/**
  * @brief Handle AWS Shadow Delata message
  * @retval None
  */
void Handle_DeltaCallback(ShadowDeviceCtx_t * pxCtx, MQTTPublishInfo_t * pxPublishInfo )
{
  static uint32_t ulCurrentVersion = 0; /* Remember the latest version number we've received */
  uint32_t ulVersion = 0UL;
  uint32_t ulNewState = 0UL;
  char * pcOutValue = NULL;
  uint32_t ulOutValueLength = 0UL;
  JSONStatus_t result = JSONSuccess;
  
  if(shadowReceived == 1)
  {
    /* Handle received delta shadow message */
    shadowReceived = 0;
    
#if EXPRESSLINK_DEBUG
    printf("[INFO] Shadow received\r\n");
    printf("[INFO] Shadow Message : %s", pxPublishInfo->pPayload);
#endif
    /* The payload will look similar to this:
    * {
    *      "state": {
    *          "powerOn": 1
    *      },
    *      "metadata": {
    *          "powerOn": {
    *              "timestamp": 1595437367
    *          }
    *      },
    *      "timestamp": 1595437367,
    *      "clientToken": "388062",
    *      "version": 12
    *  }
    */
    
    /* Make sure the payload is a valid json document. */
    result = JSON_Validate( pxPublishInfo->pPayload,  pxPublishInfo->payloadLength);
    
    if( result != JSONSuccess )
    {
#if EXPRESSLINK_DEBUG
      printf( "[ERROR] Invalid JSON document received!" );
#endif
    }
    else
    {
      /* Obtain the version value. */
      result = JSON_Search( ( char * ) pxPublishInfo->pPayload,
                           pxPublishInfo->payloadLength,
                           "version",
                           sizeof( "version" ) - 1,
                           &pcOutValue,
                           ( size_t * ) &ulOutValueLength );
      
      if( result != JSONSuccess )
      {
#if EXPRESSLINK_DEBUG
        printf( "[ERROR] Version field not found in JSON document!\r\n" );
#endif
      }
      else
      {
        /* Convert the extracted value to an unsigned integer value. */
        ulVersion = ( uint32_t ) strtoul( pcOutValue, NULL, 10 );
        
        /* Make sure the version is newer than the last one we received. */
        if( ulVersion <= ulCurrentVersion )
        {
          /* In this demo, we discard the incoming message
          * if the version number is not newer than the latest
          * that we've received before. Your application may use a
          * different approach.
          */
#if EXPRESSLINK_DEBUG
          printf( "[WARN] Received unexpected delta update with version %u. Current version is %u\r\n", ( unsigned int ) ulVersion, ( unsigned int ) ulCurrentVersion );
#endif
        }
        else
        {
#if EXPRESSLINK_DEBUG
          printf( "[INFO] Received delta update with version %.*s.\r\n", (int)ulOutValueLength, pcOutValue );
#endif
          
          /* Set received version as the current version. */
          ulCurrentVersion = ulVersion;
          
          /* Get powerOn state from json documents. */
          result = JSON_Search( ( char * ) pxPublishInfo->pPayload,
                               pxPublishInfo->payloadLength,
                               "state.desired.powerOn",
                               sizeof( "state.desired.powerOn" ) - 1,
                               &pcOutValue,
                               ( size_t * ) &ulOutValueLength );
          
          if( result != JSONSuccess )
          {
#if EXPRESSLINK_DEBUG
            printf( "[WARN] powerOn field not found in JSON document!\r\n" );
#endif
          }
          else
          {
            /* Convert the powerOn state value to an unsigned integer value. */
            ulNewState = ( uint32_t ) strtoul( pcOutValue, NULL, 10 );
            
#if EXPRESSLINK_DEBUG
            printf( "[INFO] Setting powerOn state to %u.\r\n", ( unsigned int ) ulNewState );
#endif
            /* Set the new powerOn state. */
            pxCtx->ulCurrentPowerOnState = ulNewState;
            
            if( ulNewState == 1 )
            {
              HAL_GPIO_WritePin( LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET ); /* Turn the LED ON */
            }
            else
            {
              HAL_GPIO_WritePin( LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET ); /* Turn the LED off */
            }
          }
        }
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