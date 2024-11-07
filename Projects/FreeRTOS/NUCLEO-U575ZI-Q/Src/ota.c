/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : ota.c
 * @version        : v 1.0.0
 * @brief          : ExpressLink OTA/HOTA
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

/* Includes ------------------------------------------------------------------*/
/* Includes ------------------------------------------------------------------*/

#include "main.h"
#include "ota.h"

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include "ExpressLink.h"
#include "application_version.h"

#include <stdio.h>
#include <string.h>
#include "metadata.h"
/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
/* 0: no debug, 1: Errors only, 2: INFO, 3: all (Error, Info, Debug)  */
#define DEBUG_LEVEL 2

#include "debug.h"


#if defined(FLASH_TYPEERASE_SECTORS)
#define FLASH_TYPEERASE_PAGES FLASH_TYPEERASE_SECTORS
#define Page                  Sector
#define NbPages               NbSectors
#define OPTR                  OPTCR
#endif
/* Private variables ---------------------------------------------------------*/
extern SemaphoreHandle_t ExpressLink_MutexHandle;
extern EventGroupHandle_t ExpressLink_EventHandle;

static uint32_t ota_data[OTA_BUFFER_SIZE / 4];

/* Private functions ---------------------------------------------------------*/
static HAL_StatusTypeDef ota_flash_erase(void);
static HAL_StatusTypeDef ota_flash_write(const uint32_t address, uint8_t *data, const uint32_t size);
static void ota_flash_swap_bank(void);
static void ota_do_hota(void);

/* User code ---------------------------------------------------------*/
void vOTATask(void *argument)
{
#if (DEBUG_LEVEL > 0)
  setvbuf(stdout, NULL, _IONBF, 0); /* Disable buffering for stdout */
#endif

  PRINTF("[INFO] OTA Task started\r\n");
  PRINTF_INFO("[INFO] Application version %d.%d.%d\r\n", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);

#if defined(SWAP_BANK_ENABLE)
  if (FLASH->OPTR & SWAP_BANK_ENABLE)
  {
    PRINTF_INFO("[INFO] Running from BANK 2\r\n");
  }
  else
  {
    PRINTF_INFO("[INFO] Running from BANK 1\r\n");
  }
#else
  PRINTF_INFO("[INFO] Single bank device\r\n");
#endif

  for (;;)
  {
    xEventGroupWaitBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_OTA, pdTRUE, pdTRUE, portMAX_DELAY);

    xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);

    switch (ExpressLink_OTA_GetState())
    {
    /**********************************************/
    case NO_OTA_IN_PROGRESS:
      PRINTF_INFO("[INFO] No OTA in progress\r\n");

      ExpressLink_OTA_Flush();
      break;

      /**********************************************/
    case NEW_MODULE_OTA_PROPOSED:
      PRINTF_INFO("[INFO] A new module OTA update is being proposed\r\n");

      ExpressLink_OTA_Accept();
      break;

      /**********************************************/
    case NEW_HOST_OTA_PROPOSED:
      PRINTF_INFO("[INFO] A new Host OTA update is being proposed\r\n");
      ExpressLink_OTA_Accept();
      break;

      /**********************************************/
    case OTA_IN_PROGRESS:
      PRINTF_INFO("[INFO] OTA in progress.\r\n");
      break;

      /**********************************************/
    case NEW_MODULE_IMAGE_ARRIVED:
      PRINTF_INFO("[INFO] A new module firmware image has arrived.\r\n");

      ExpressLink_OTA_Close();
      ExpressLink_OTA_Apply();
      vTaskDelay(pdMS_TO_TICKS(DELAY_OTA_CLOSE));

      HAL_NVIC_SystemReset();
      break;

      /**********************************************/
    case NEW_HOST_IMAGE_ARRIVED:
      PRINTF_INFO("[INFO] A new Host image has arrived.\r\n");

      taskENTER_CRITICAL(); /* We need to suspend interrupts to not disturb USART communication and risk to corrupt received messages */
      ota_do_hota();
      taskEXIT_CRITICAL();
      break;

      /**********************************************/
    default:
      ExpressLink_OTA_Flush();
      break;
    }

    xSemaphoreGive(ExpressLink_MutexHandle);
  }
}

static void ota_do_hota(void)
{
#if defined(HAL_ICACHE_MODULE_ENABLED)
  HAL_ICACHE_Disable();
#endif

  int ota_state = 0;
  uint32_t address = HOTA_START_ADDRESS; //FLASH_START_ADDRESS + FLASH_BANK_SIZE;
  ExpressLink_OTA_Data_t OTA_Data;
  uint32_t hota_image_size = 0;

#if defined(__STM32H5xx_HAL_H) || defined(STM32L5xx_HAL_H)
  uint32_t saved_flash_latency = __HAL_FLASH_GET_LATENCY();
  __HAL_FLASH_SET_LATENCY(__HAL_FLASH_GET_LATENCY() + 2);
  (void)__HAL_FLASH_GET_LATENCY();
#endif


  OTA_Data.data = ota_data;

  if (ota_flash_erase() != HAL_OK)
  {
	PRINTF_ERROR("[ERROR] Flash Erase error\r\n");

    ExpressLink_OTA_Flush();

#if defined(HAL_ICACHE_MODULE_ENABLED)
    HAL_ICACHE_Enable();
#endif

    return;
  }

  do
  {
    if (ExpressLink_OTA_Read(&OTA_Data, OTA_BUFFER_SIZE) < OTA_BUFFER_SIZE)
    {
      hota_image_size -= OTA_BUFFER_SIZE;
      hota_image_size += OTA_Data.count;
    }

    if (hota_image_size <= MAX_HOTA_IMAGE_SIZE)
    {
      ota_flash_write(address, (uint8_t*) ota_data, OTA_Data.count);
    }

    if (OTA_Data.count == OTA_BUFFER_SIZE)
    {
      address += OTA_BUFFER_SIZE;
      hota_image_size += OTA_BUFFER_SIZE;
    }

    PRINTF_INFO("\r[INFO] hota_image_size %d", (int ) hota_image_size);
  }
  while (OTA_Data.count == OTA_BUFFER_SIZE);

  if (hota_image_size <= MAX_HOTA_IMAGE_SIZE)
  {
    ExpressLink_OTA_Close();

    PRINTF_INFO("\n");

    do
    {
      ota_state = ExpressLink_OTA_GetState();
      PRINTF_INFO(".");
      PRINTF_DEBUG("[DEBUG] OTA State %d\r\n", ota_state);
    }
    while (ota_state != 0);

    PRINTF_INFO("\r\n");

    taskEXIT_CRITICAL();

    PRINTF_INFO("[INFO] Rebooting\n");

    vTaskDelay(pdMS_TO_TICKS(2000));
    ota_flash_swap_bank();
  }
  else
  {
    PRINTF_ERROR("[ERROR] HOTA Image 0x%08X exceeds MAX_HOTA_IMAGE_SIZE 0x%08X \r\n", (int ) hota_image_size, (int) MAX_HOTA_IMAGE_SIZE);

    ExpressLink_OTA_Flush();
  }

#if defined(__STM32H5xx_HAL_H) || defined(STM32L5xx_HAL_H)
  __HAL_FLASH_SET_LATENCY(saved_flash_latency);
  (void)__HAL_FLASH_GET_LATENCY();;
#endif

#if defined(HAL_ICACHE_MODULE_ENABLED)
  HAL_ICACHE_Enable();
#endif
}

static HAL_StatusTypeDef ota_flash_write(const uint32_t address, uint8_t *data, const uint32_t size)
{
  HAL_StatusTypeDef status = HAL_OK;

  PRINTF_DEBUG("\r\n[DEBUG] Writing to address 0x%08X\n", (int)address);

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

static HAL_StatusTypeDef ota_flash_erase(void)
{
  uint32_t PageError = 0;
  HAL_StatusTypeDef status = HAL_OK;
  FLASH_EraseInitTypeDef EraseInitStruct;
  FLASH_OBProgramInitTypeDef OBInit;

  HAL_FLASH_Unlock();

  HAL_FLASHEx_OBGetConfig(&OBInit);

#if defined(FLASH_BANK_2)
  EraseInitStruct.Page      = 0;
#else
  EraseInitStruct.Page      = HOTA_START_PAGE;
#endif
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  EraseInitStruct.NbPages   = MAX_HOTA_IMAGE_SIZE / FLASH_PAGE_SIZE;

#if defined(FLASH_BANK_2)
  if (FLASH->OPTR & SWAP_BANK_ENABLE)
  {
    PRINTF_DEBUG("[DEBUG] Running from BANK 2, Erasing BANK1\r\n");
#if defined (STM32G0xx_HAL_H)
    EraseInitStruct.Banks = FLASH_BANK_2;
#else
    EraseInitStruct.Banks = FLASH_BANK_1;
#endif
  }
  else
  {
    PRINTF_DEBUG("[DEBUG] Running from BANK 1, Erasing BANK2\r\n");

#if defined (STM32G0xx_HAL_H)
    EraseInitStruct.Banks = FLASH_BANK_1;
#else
    EraseInitStruct.Banks = FLASH_BANK_2;
#endif
  }
#endif /* defined(FLASH_BANK_2) */


  status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

  HAL_FLASH_Lock();

  return status;
}

static void ota_flash_swap_bank(void)
{
#if (SECURE_BOOT == 1) || !defined(FLASH_BANK_2)
  PRINTF_INFO("[INFO] Secure Boot required to install new HOTA\r\n");
  vTaskDelay(pdMS_TO_TICKS(10));
  HAL_NVIC_SystemReset();
#else
  FLASH_OBProgramInitTypeDef OBInit;

  HAL_FLASH_Unlock();

  HAL_FLASH_OB_Unlock();

  /* Get the boot configuration status */
  HAL_FLASHEx_OBGetConfig(&OBInit);

  /* Check Swap Flash banks  status */
  if (OBInit.USERConfig & SWAP_BANK_ENABLE)
  {
    PRINTF_DEBUG("[DEBUG] Switching to Bank 1\r\n");

    /*Swap to bank2 */
    OBInit.USERConfig = SWAP_BANK_DISABLE;
  }
  else
  {
    PRINTF_DEBUG("[DEBUG] Switching to Bank 2\r\n");

    /*Swap to bank2 */
    OBInit.USERConfig = SWAP_BANK_ENABLE;
  }

  OBInit.OptionType = OPTIONBYTE_USER;
  OBInit.USERType = USER_SWAP_BANK;

  HAL_FLASHEx_OBProgram(&OBInit);

  /* Launch Option bytes loading */
  HAL_FLASH_OB_Launch();

  HAL_FLASH_Lock();

#if defined(__STM32H5xx_HAL_H)
  HAL_NVIC_SystemReset();
#endif /* __STM32H5xx_HAL_H */
#endif /* defined(SECURE_BOOT) || !defined(FLASH_BANK_2) */
}
