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
#include <string.h>
#include "cmox_crypto.h"
#include "SigGen.h"
#include "header.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

//#define DEBUG 0
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

TIM_HandleTypeDef htim16;

/* USER CODE BEGIN PV */
/* Private typedef -----------------------------------------------------------*/
typedef void (*pFunction)(void);

/* Private defines -----------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* ECC context */
cmox_ecc_handle_t Ecc_Ctx;

/* ECC working buffer */
uint8_t Working_Buffer[2000];

const uint8_t *const aInput = (uint8_t*) APPLICATION_START_ADDRESS;
uint8_t Public_Key[CMOX_ECC_SECP256R1_PUBKEY_LEN] = { 0 };

header_t app1Header;

pFunction JumpToApplication;
uint32_t JumpAddress;

/* Computed data buffer */
uint8_t Computed_Hash[CMOX_SHA256_SIZE];

size_t computed_size;
/* Fault check verification variable */
uint32_t fault_check = CMOX_ECC_AUTH_FAIL;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
void Error_Handler(void);

/* Functions Definition ------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CRC_Init(void);
static void MX_TIM16_Init(void);
/* USER CODE BEGIN PFP */
static HAL_StatusTypeDef ota_flash_erase(uint8_t app);
static HAL_StatusTypeDef ota_flash_write(const uint32_t address, uint8_t *data, const uint32_t size);
static uint8_t image_signature_verify(uint32_t size, const uint8_t *aInput);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
    status = HAL_FLASH_Program(FLASH_PROGRAM_SIZE, address + i, x[i / FLASH_ADDRESS_INC_SIZE]);
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
    EraseInitStruct.Page    = HOTA_START_PAGE;
#if defined (FLASH_OPTR_DUALBANK)
    EraseInitStruct.Banks   = 2;
#endif

    status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
  }
  else if (app == APP)
  {
    EraseInitStruct.NbPages   = (MAX_HOTA_IMAGE_SIZE / FLASH_PAGE_SIZE);
    EraseInitStruct.Page      = APP_START_PAGE;
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

static uint8_t image_signature_verify(uint32_t size, const uint8_t *aInput)
{
  /* Initialize cryptographic library */
  cmox_initialize(NULL);

  if (cmox_hash_compute(CMOX_SHA256_ALGO, aInput, size, Computed_Hash, CMOX_SHA256_SIZE, &computed_size) != CMOX_HASH_SUCCESS)
  {
    return HEADER_ERROR;
  }

  cmox_ecc_construct(&Ecc_Ctx, CMOX_ECC256_MATH_FUNCS, Working_Buffer, sizeof(Working_Buffer));

  for (int i = 0; i < 32; i++)
  {
    Public_Key[i     ] = SigGen_Qx[i];
    Public_Key[i + 32] = SigGen_Qy[i];
  }

  const uint8_t *const Computed_Signature = (uint8_t*) (aInput + size);

  cmox_ecc_retval_t ERR = cmox_ecdsa_verify(&Ecc_Ctx, CMOX_ECC_CURVE_SECP256R1, Public_Key, CMOX_ECC_SECP256R1_PUBKEY_LEN, Computed_Hash, CMOX_SHA256_SIZE, Computed_Signature, CMOX_ECC_SECP256R1_SIG_LEN, &fault_check);

  if (ERR != CMOX_ECC_AUTH_SUCCESS)
  {
    return HEADER_ERROR;
  }

  /* Cleanup context */
  cmox_ecc_cleanup(&Ecc_Ctx);

  /* No more need of cryptographic services, finalize cryptographic library */
  cmox_finalize(NULL);

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
  MX_CRC_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */

  // Check Header version and construct app1Header
  if (header_get_data(APP_HEADER_START_ADDRESS, &app1Header) == HEADER_ERROR)
  {
    while (1)
      ;
  }

  // Check app1Header size, board, and revision
  if (validate_header(&app1Header) == HEADER_ERROR)
  {
    while (1)
      ;
  }

  // Check App one signature
  uint8_t status = image_signature_verify(atoi(app1Header.pSize), (const uint8_t*) APP_HEADER_START_ADDRESS);
  if (status == HEADER_ERROR)
  {
    while (1)
      ;
  }

  // Check for a valid OTA
  header_t app2Header;

  if (header_get_data(HOTA_HEADER_START_ADDRESS, &app2Header) == HEADER_SUCCESS)
  {
    if (validate_ota(&app1Header, &app2Header) == HEADER_ERROR)
    {
      status = ota_flash_erase(2);

      if (status == HAL_OK)
      {
        NVIC_SystemReset();
      }
    }

    // Check App two signature
    status = image_signature_verify(atoi(app2Header.pSize), (const uint8_t*) HOTA_HEADER_START_ADDRESS);

    if (status == HEADER_ERROR)
    {
      status = ota_flash_erase(HOTA);

      if (status == HAL_OK)
      {
        NVIC_SystemReset();
      }
    }

    status = ota_flash_erase(APP);

    status = ota_flash_write((const uint32_t) APP_HEADER_START_ADDRESS, (uint8_t*) HOTA_HEADER_START_ADDRESS, (const uint32_t) MAX_HOTA_IMAGE_SIZE);

    status = ota_flash_erase(HOTA);

    // RESET
    if (status == HAL_OK)
    {
      NVIC_SystemReset();
    }
    else
    {
      while (1)
        ;
    }
  }

  HAL_NVIC_DisableIRQ(TIM16_IRQn);
  HAL_TIM_Base_DeInit(&htim16);
  __HAL_RCC_TIM16_FORCE_RESET();
  HAL_SuspendTick();
  HAL_DeInit();
  __HAL_RCC_TIM16_RELEASE_RESET();

  /* ECC ECDSA operations are successful */
  {
    /* Jump to user application */
    JumpAddress = *(__IO uint32_t*) (APPLICATION_START_ADDRESS + 4 + 0x200);
    JumpToApplication = (pFunction) JumpAddress;

    /* Initialize user application's Stack Pointer */
    __set_MSP(*(__IO uint32_t*) APPLICATION_START_ADDRESS + 0x200);

    JumpToApplication();
  }

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
  RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

  /** Configure the main internal regulator output voltage
   */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief CRC Initialization Function
 * @param None
 * @retval None
 */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
 * @brief TIM16 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  TIM_IC_InitTypeDef sConfigIC = { 0 };

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 0;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 65535;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim16, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIMEx_TISelection(&htim16, TIM_TIM16_TI1_LSI, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */

}

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
