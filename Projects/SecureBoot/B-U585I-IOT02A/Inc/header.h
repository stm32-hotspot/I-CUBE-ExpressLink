/* USER CODE BEGIN Header */
/**
******************************************************************************
* @file           : header.h
* @brief          :
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

#ifndef ST_IMAGEHEADER_HEADER_H_
#define ST_IMAGEHEADER_HEADER_H_

#include "main.h"

#define BOOT_HEADER  "SECUREBOOT10" /* Header Secure boot revision 1.0  */

#define APP  1
#define HOTA 2

#define HEADER_SUCCESS        0
#define HEADER_ERROR          (!HEADER_SUCCESS)

#define SECURE_BOOT_SIZE      0x00008000
#define HEADER_OFFSET         0x00000200

#define MAX_LENGTH_HEADER     0x10
#define MAX_LENGTH_SIZE       0x10
#define MAX_LENGTH_BOARD      0x10
#define MAX_LENGTH_REV        0x10

#define HEADER_ADDRESS        0
#define SIZE_ADDRESS          (HEADER_ADDRESS     + MAX_LENGTH_HEADER )
#define BOARD_NAME_ADDRESS    (SIZE_ADDRESS       + MAX_LENGTH_SIZE   )
#define REVISION_ADDRESS      (BOARD_NAME_ADDRESS + MAX_LENGTH_BOARD  )

#if defined(__STM32H5xx_HAL_H)
      #define MAX_HOTA_IMAGE_SIZE           ((FLASH_SIZE/2) - SECURE_BOOT_SIZE - FLASH_SECTOR_SIZE)
      #define OTA_START_SECTOR              ( FLASH_SIZE/2)/FLASH_SECTOR_SIZE
      #define HOTA_START_ADDRESS            ((FLASH_SIZE/2) + FLASH_START_ADDRESS)
      #define APP_START_PAGE                (SECURE_BOOT_SIZE/FLASH_PAGE_SIZE)
      #define APP_HEADER_START_ADDRESS  ( FLASH_BASE + SECURE_BOOT_SIZE)
      #define HOTA_HEADER_START_ADDRESS  HOTA_START_ADDRESS
#elif defined(STM32WBxx_HAL_H)
      #define MAX_HOTA_IMAGE_SIZE           ((FLASH_SIZE/4) - FLASH_PAGE_SIZE- SECURE_BOOT_SIZE)/* Need to count for the radio stack. This is a quick fix */
      #define HOTA_START_PAGE               ((SECURE_BOOT_SIZE + MAX_HOTA_IMAGE_SIZE) / FLASH_PAGE_SIZE)
      #define HOTA_START_ADDRESS            ( FLASH_START_ADDRESS + SECURE_BOOT_SIZE + MAX_HOTA_IMAGE_SIZE)
      #define APP_START_PAGE                (SECURE_BOOT_SIZE/FLASH_PAGE_SIZE)
      #define APP_HEADER_START_ADDRESS  ( FLASH_BASE + SECURE_BOOT_SIZE)
      #define HOTA_HEADER_START_ADDRESS  HOTA_START_ADDRESS
 #else
    #if !defined(FLASH_BANK_2)
      #define MAX_HOTA_IMAGE_SIZE           ((FLASH_SIZE - (2 * FLASH_PAGE_SIZE)- SECURE_BOOT_SIZE)/2)
      #define HOTA_START_PAGE               ((SECURE_BOOT_SIZE + MAX_HOTA_IMAGE_SIZE) / FLASH_PAGE_SIZE)
      #define HOTA_START_ADDRESS            ( FLASH_START_ADDRESS + SECURE_BOOT_SIZE + MAX_HOTA_IMAGE_SIZE)
      #define APP_START_PAGE                (SECURE_BOOT_SIZE/FLASH_PAGE_SIZE)
      #define APP_HEADER_START_ADDRESS      ( FLASH_BASE + SECURE_BOOT_SIZE)
      #define HOTA_HEADER_START_ADDRESS     ( FLASH_BASE + SECURE_BOOT_SIZE + MAX_HOTA_IMAGE_SIZE)
    #else
      #define MAX_HOTA_IMAGE_SIZE           ((FLASH_SIZE/2) - SECURE_BOOT_SIZE - FLASH_PAGE_SIZE)
      #define HOTA_START_PAGE               ( FLASH_SIZE/2)/FLASH_PAGE_SIZE
      #define HOTA_START_ADDRESS            ((FLASH_SIZE/2) + FLASH_BASE)
      #define APP_START_PAGE                (SECURE_BOOT_SIZE/FLASH_PAGE_SIZE)
      #define APP_HEADER_START_ADDRESS      ( FLASH_BASE + SECURE_BOOT_SIZE)
      #define HOTA_HEADER_START_ADDRESS     HOTA_START_ADDRESS
    #endif /* FLASH_BANK_2 */
#endif /* __STM32H5xx_HAL_H */

#define APPLICATION_START_ADDRESS     (FLASH_BASE + SECURE_BOOT_SIZE)

#if defined(FLASH_TYPEPROGRAM_QUADWORD)/* H5 and U5 */
  #define FLASH_PROGRAM_SIZE           FLASH_TYPEPROGRAM_QUADWORD
  #define FLASH_ADDRESS_INC_SIZE       (16)
  #ifndef FLASH_DBANK_SUPPORT
    #define FLASH_DBANK_SUPPORT
  #endif
#else
  #define FLASH_PROGRAM_SIZE           FLASH_TYPEPROGRAM_DOUBLEWORD
  #define FLASH_ADDRESS_INC_SIZE       (8)
#endif


typedef struct header_t
{
  char *pSize;
  char *pBoard;
  char *pRev;
}header_t;

uint8_t validate_header(header_t *pHEADER);
uint8_t validate_ota(header_t *app1, header_t *app2);
uint8_t header_get_data(uint32_t address, header_t *pHEADER);

#endif /* ST_IMAGEHEADER_HEADER_H_ */
