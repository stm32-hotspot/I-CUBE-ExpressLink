/* USER CODE BEGIN Header */
/**
******************************************************************************
* @file           : ota.h
* @version        : v 1.0.0
* @brief          : ExpressLink OTA task
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __OTA_H__
#define __OTA_H__

#ifdef __cplusplus
extern "C" {
#endif
  
/* Includes ------------------------------------------------------------------*/
#include "ExpressLink.h"

/* Exported types ------------------------------------------------------------*/


/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/
#define OTA_BUFFER_SIZE  512    /* Specify how many HOTA bytes to read from  the module at a time. Keep in mind the binary data is sent as string */
#define DELAY_OTA_CLOSE  20000  /* Delay after sending the AT+OTA CLOSE command to end the OTA Job. */
#define METADATA_SIZE    FLASH_PAGE_SIZE

#if (SECURE_BOOT)
  #define SECURE_BOOT_SIZE 0x8000 /* 32K */
#else
  #define SECURE_BOOT_SIZE 0x0000 /* 0K */
#endif

#if (((OTA_BUFFER_SIZE * 2) + 14) > EXPRESSLINK_RX_BUFFER_SIZE)
#error "OTA_BUFFER_SIZE too big"
#endif

#if defined(__STM32H5xx_HAL_H)
#define FLASH_PAGE_SIZE FLASH_SECTOR_SIZE
#endif

#if !defined(FLASH_BANK_2) /* If single bank */
  #if defined(STM32WBxx_HAL_H)
      #define MAX_HOTA_IMAGE_SIZE           ((FLASH_SIZE/4) - METADATA_SIZE- SECURE_BOOT_SIZE)/* Need to count for the radio stack. This is a quick fix */
  #else
    #define MAX_HOTA_IMAGE_SIZE           (((FLASH_SIZE - SECURE_BOOT_SIZE)/2) - METADATA_SIZE)
  #endif

  #define HOTA_START_PAGE               ((SECURE_BOOT_SIZE + MAX_HOTA_IMAGE_SIZE) / FLASH_PAGE_SIZE)
  #define HOTA_START_ADDRESS            ( FLASH_START_ADDRESS + SECURE_BOOT_SIZE + MAX_HOTA_IMAGE_SIZE)
#else
  #define MAX_HOTA_IMAGE_SIZE           ((FLASH_SIZE/2) - SECURE_BOOT_SIZE - METADATA_SIZE)
  #define HOTA_START_PAGE               ( FLASH_SIZE/2)/FLASH_PAGE_SIZE
  #define HOTA_START_ADDRESS            ((FLASH_SIZE/2) + FLASH_BASE)
#endif /* FLASH_BANK_2 */

#define APP_START_PAGE                ( SECURE_BOOT_SIZE/FLASH_PAGE_SIZE)
#define APP_HEADER_START_ADDRESS      ( FLASH_BASE + SECURE_BOOT_SIZE)
#define HOTA_HEADER_START_ADDRESS       HOTA_START_ADDRESS

#define NO_OTA_IN_PROGRESS       0
#define NEW_MODULE_OTA_PROPOSED  1
#define NEW_HOST_OTA_PROPOSED    2
#define OTA_IN_PROGRESS          3
#define NEW_MODULE_IMAGE_ARRIVED 4
#define NEW_HOST_IMAGE_ARRIVED   5
  
/* Exported functions prototypes ---------------------------------------------*/

void vOTATask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __OTA_H__ */
