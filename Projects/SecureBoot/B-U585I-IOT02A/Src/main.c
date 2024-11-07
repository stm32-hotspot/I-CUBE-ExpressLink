/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    PKA/PKA_ECDSA_Sign/Src/main.c
 * @author  MCD Application Team
 * @brief   This example describes how to configure and use PKA through
 *          the STM32U5xx HAL API.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021 STMicroelectronics.
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
#include "SigGen.h"
#include "prime256v1.h"
#include "header.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef void (*pFunction)(void);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define DEBUG 1

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

HASH_HandleTypeDef hhash;

PKA_HandleTypeDef hpka;

RNG_HandleTypeDef hrng;

#if (DEBUG)
UART_HandleTypeDef huart1;
#endif

/* USER CODE BEGIN PV */
PKA_ECDSASignInTypeDef  in     = { 0 };
PKA_ECDSASignOutTypeDef out    = { 0 };
PKA_ECDSAVerifInTypeDef ver_in = { 0 };
uint8_t SHA256Digest[32]       = { 0 };

header_t app1Header;
header_t app2Header;

pFunction JumpToApplication;
uint32_t JumpAddress;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void SystemPower_Config(void);
static void MX_GPIO_Init(void);
static void MX_HASH_Init(void);
static void MX_ICACHE_Init(void);
static void MX_RNG_Init(void);
static void MX_PKA_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
uint32_t PKA_ECDSAVerify(const uint8_t *phash, const PKA_ECDSASignOutTypeDef *pout);
static HAL_StatusTypeDef ota_flash_erase(uint8_t app);
static HAL_StatusTypeDef ota_flash_write(const uint32_t address, uint8_t *data, const uint32_t size);
#if (DEBUG)
void print_results(void);
#endif
uint8_t image_signature_verify(uint32_t size, const uint8_t *aInput);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint32_t PKA_ECDSAVerify(const uint8_t *phash, const PKA_ECDSASignOutTypeDef *pout)
{

  /* Launch the signature verification verification */
#if defined(PKA) && defined(HAL_PKA_MODULE_ENABLED)
  ver_in.primeOrderSize = prime256v1_Order_len;
  ver_in.modulusSize = prime256v1_Prime_len;
  ver_in.coefSign = prime256v1_A_sign;
  ver_in.coef = prime256v1_absA;

  ver_in.modulus = prime256v1_Prime;
  ver_in.basePointX = prime256v1_GeneratorX;
  ver_in.basePointY = prime256v1_GeneratorY;
  ver_in.primeOrder = prime256v1_Order;

  ver_in.pPubKeyCurvePtX = SigGen_Qx;
  ver_in.pPubKeyCurvePtY = SigGen_Qy;
  ver_in.RSign = pout->RSign;
  ver_in.SSign = pout->SSign;

  ver_in.hash = phash;
  HAL_PKA_ECDSAVerif(&hpka, &ver_in, 5000);

  return HAL_PKA_ECDSAVerif_IsValidSignature(&hpka);
#elif
  printf('USING CRYPTO LIB');

  return 1;

#endif
}

static HAL_StatusTypeDef ota_flash_write(const uint32_t address, uint8_t *data, const uint32_t size)
{
  HAL_StatusTypeDef status = HAL_OK;

  HAL_FLASH_Unlock();

#if (FLASH_ADDRESS_INC_SIZE == 8)
  uint64_t *x = (uint64_t*) data;
#endif

  for (int i = 0; i < size; i += FLASH_ADDRESS_INC_SIZE)
  {
    FLASH_WaitForLastOperation(1000);

#if (FLASH_ADDRESS_INC_SIZE == 8)
    status =  HAL_FLASH_Program(FLASH_PROGRAM_SIZE, address + i, x[i/FLASH_ADDRESS_INC_SIZE]);
#else
    status = HAL_FLASH_Program(FLASH_PROGRAM_SIZE, address + i, (uint32_t) data + i);
#endif
    if (status != HAL_OK)
    {
      break;
    }
  }

  HAL_FLASH_Lock();

  return status;
}

static HAL_StatusTypeDef ota_flash_erase(uint8_t app)
{
  HAL_StatusTypeDef status = HAL_OK;
  FLASH_EraseInitTypeDef EraseInitStruct;
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  uint32_t PageError = 0;

#if defined(HAL_ICACHE_MODULE_ENABLED)
  HAL_ICACHE_Disable();
#endif

  HAL_FLASH_Unlock();

  if (app == HOTA)
  {
	  EraseInitStruct.NbPages = (MAX_HOTA_IMAGE_SIZE / FLASH_PAGE_SIZE);
    EraseInitStruct.Page 		= HOTA_START_PAGE;
#if defined (FLASH_OPTR_DUALBANK)
    EraseInitStruct.Banks   = 2;
#endif

    status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
  }
  else if (app == APP)
  {
    EraseInitStruct.NbPages   = (MAX_HOTA_IMAGE_SIZE / FLASH_PAGE_SIZE);
    EraseInitStruct.Page      =  APP_START_PAGE;
#if defined(FLASH_OPTR_DUALBANK)
    EraseInitStruct.Banks     = 1;
#endif

	  status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
  }

  HAL_FLASH_Lock();

#if defined(HAL_ICACHE_MODULE_ENABLED)
  HAL_ICACHE_Enable();
#endif

  return status;
}

#if (DEBUG)
void print_results(void)
{
  //printf("Private key : 0x"); for(int i = 0; i < prime256v1_Order_len; i++) {printf("%02X", SigGen_d    [i]);} printf("\r\n");
  printf("qx          : 0x");
  for (int i = 0; i < prime256v1_Order_len; i++)
  {
    printf("%02X", SigGen_Qx[i]);
  }
  printf("\r\n");
  printf("qy          : 0x");
  for (int i = 0; i < prime256v1_Order_len; i++)
  {
    printf("%02X", SigGen_Qy[i]);
  }
  printf("\r\n");
  printf("SHA256Digest: 0x");
  for (int i = 0; i < 32; i++)
  {
    printf("%02X", SHA256Digest[i]);
  }
  printf("\r\n");
  printf("r_hex       : 0x");
  for (int i = 0; i < prime256v1_Order_len; i++)
  {
    printf("%02X", out.RSign[i]);
  }
  printf("\r\n");
  printf("s_hex       : 0x");
  for (int i = 0; i < prime256v1_Order_len; i++)
  {
    printf("%02X", out.SSign[i]);
  }
  printf("\r\n");
}
#endif

uint8_t image_signature_verify(uint32_t size, const uint8_t *aInput)
{
  /* Calculate hash */
  HAL_HASHEx_SHA256_Start(&hhash, (uint8_t*) aInput, size, SHA256Digest, 0xFF);

  /* Collect signature */
  out.RSign = (uint8_t*) (aInput + size);
  out.SSign = (uint8_t*) (aInput + size + 32);

#if (DEBUG)
  //print_results();
#endif

  /* Verify the signature */
  if (!PKA_ECDSAVerify(SHA256Digest, &out))
  {
    return HEADER_ERROR;
  }

  return HEADER_SUCCESS;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* STM32U5xx HAL library initialization:
   - Configure the Flash prefetch
   - Configure the Systick to generate an interrupt each 1 msec
   - Set NVIC Group Priority to 3
   - Low Level Initialization
   */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

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
  MX_HASH_Init();
  MX_ICACHE_Init();
  MX_RNG_Init();
  MX_PKA_Init();
#if (DEBUG)
  MX_USART1_UART_Init();
#endif
  /* USER CODE BEGIN 2 */

#if (DEBUG)
    printf("[INFO] APP_HEADER_START_ADDRESS 0x%08X\n", (int)APP_HEADER_START_ADDRESS);
#endif
  // Check Header version and construct app1Header
  if (header_get_data(APP_HEADER_START_ADDRESS, &app1Header) == HEADER_ERROR)
  {
#if (DEBUG)
    printf("[ERROR] Application boot header verification failed\r\n");
#endif
    while (1)
      ;
  }

#if (DEBUG)
  printf("[INFO] Application boot header version success\r\n");
#endif

  // Check app1Header size, board and revision
  if (validate_header(&app1Header) == HEADER_ERROR)
  {
#if (DEBUG)
    printf("[ERROR] Application size, board, and revision validation failed\r\n");
#endif
    while (1)
    {
      ;
    }
  }

#if (DEBUG)
  printf("[INFO] Application size, board, and revision verification successful\r\n");
#endif

  // Check App one signature
  uint8_t status = image_signature_verify(atoi(app1Header.pSize), (const uint8_t*) APP_HEADER_START_ADDRESS);

  if (status == HEADER_ERROR)
  {
#if (DEBUG)
    printf("[ERROR] Application signature verification failed\r\n");
#endif
    while (1)
    {
      ;
    }
  }

#if (DEBUG)
  printf("[INFO] Application signature verification successful\r\n");
#endif

#if (DEBUG)
  printf("[INFO] HOTA_HEADER_START_ADDRESS 0x%08X\n", (int)HOTA_HEADER_START_ADDRESS);
#endif

  // Check for a valid OTA
  if (header_get_data(HOTA_HEADER_START_ADDRESS, &app2Header) == HEADER_SUCCESS)
  {
#if (DEBUG)
    printf("[INFO] HOTA available\r\n");
#endif

    if (validate_ota(&app1Header, &app2Header) == HEADER_ERROR)
    {
#if (DEBUG)
      printf("[ERROR] HOTA boot header verification failed\r\n");
#endif

      status = ota_flash_erase(HOTA);

      if (status == HAL_OK)
      {
        NVIC_SystemReset();
      }
    }

    // Check App two signature
    status = image_signature_verify(atoi(app2Header.pSize), (const uint8_t*) HOTA_HEADER_START_ADDRESS);

    if (status == HEADER_ERROR)
    {
#if (DEBUG)
      printf("[ERROR] HOTA signature verification failed\r\n");
      printf("[INFO] Erasing HOTA\r\n");
#endif

      status = ota_flash_erase(HOTA);

      if (status == HAL_OK)
      {
        NVIC_SystemReset();
      }
    }

#if (DEBUG)
    printf("[INFO] HOTA signature verification successful\r\n");
    printf("[INFO] Valid HOTA available\r\n");
#endif

    //Erasing App one
#if (DEBUG)
    printf("[INFO] Erasing Application\r\n");
#endif
    status = ota_flash_erase(APP);

    // Copy app two in place of app one
#if (DEBUG)
    printf("[INFO] Copying HOTA in place of Application\r\n");
#endif
    status = ota_flash_write((const uint32_t) APP_HEADER_START_ADDRESS, (uint8_t*) HOTA_HEADER_START_ADDRESS, (const uint32_t) MAX_HOTA_IMAGE_SIZE);

    // Erase APP two
#if (DEBUG)
    printf("[INFO] Erasing HOTA\r\n");
#endif
    status = ota_flash_erase(HOTA);

    // RESET
    if (status == HAL_OK)
    {
      NVIC_SystemReset();
    }
  }
  else
  {
#if (DEBUG)
    printf("[INFO] No HOTA Available\r\n");
    printf("[INFO] Booting Application\r\n");
#endif
  }

  HAL_SuspendTick();
  HAL_RNG_DeInit(&hrng);
  HAL_PKA_DeInit(&hpka);
  HAL_HASH_DeInit(&hhash);
  HAL_ICACHE_Disable();
  HAL_ICACHE_Invalidate();

  /* Test if user code is programmed starting from address 0x0800C000 */
  uint32_t StackPointer = *((uint32_t*) APP_HEADER_START_ADDRESS + HEADER_OFFSET);

  StackPointer &= 0x2FF00000;

  /* Jump to user application */
  JumpAddress = *(uint32_t*) (APP_HEADER_START_ADDRESS + 4 + HEADER_OFFSET);
  JumpToApplication = (pFunction) JumpAddress;

  /* Initialize user application's Stack Pointer */
  __set_MSP(*(uint32_t*) APP_HEADER_START_ADDRESS + HEADER_OFFSET);

  JumpToApplication();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
#if (DEBUG)
    __BKPT(0);
#endif
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_0;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV4;
  RCC_OscInitStruct.PLL.PLLM = 3;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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
  HAL_PWREx_DisableUCPDDeadBattery();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

/**
  * @brief HASH Initialization Function
  * @param None
  * @retval None
  */
static void MX_HASH_Init(void)
{

  /* USER CODE BEGIN HASH_Init 0 */

  /* USER CODE END HASH_Init 0 */

  /* USER CODE BEGIN HASH_Init 1 */

  /* USER CODE END HASH_Init 1 */
  hhash.Init.DataType = HASH_DATATYPE_8B;
  if (HAL_HASH_Init(&hhash) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN HASH_Init 2 */

  /* USER CODE END HASH_Init 2 */

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
  * @brief PKA Initialization Function
  * @param None
  * @retval None
  */
static void MX_PKA_Init(void)
{

  /* USER CODE BEGIN PKA_Init 0 */

  /* USER CODE END PKA_Init 0 */

  /* USER CODE BEGIN PKA_Init 1 */

  /* USER CODE END PKA_Init 1 */
  hpka.Instance = PKA;
  if (HAL_PKA_Init(&hpka) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN PKA_Init 2 */

  /* USER CODE END PKA_Init 2 */

}

/**
  * @brief RNG Initialization Function
  * @param None
  * @retval None
  */
static void MX_RNG_Init(void)
{

  /* USER CODE BEGIN RNG_Init 0 */

  /* USER CODE END RNG_Init 0 */

  /* USER CODE BEGIN RNG_Init 1 */

  /* USER CODE END RNG_Init 1 */
  hrng.Instance = RNG;
  hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
  if (HAL_RNG_Init(&hrng) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RNG_Init 2 */

  /* USER CODE END RNG_Init 2 */

}
#if (DEBUG)
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
#endif
/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while (1)
  {
    /* Error if LED7 is slowly blinking (1 sec. period) */
    HAL_Delay(1000);
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

  /* Infinite loop */
  while (1)
  {
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
