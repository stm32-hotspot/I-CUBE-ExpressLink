/*
 * Header.c
 *
 *  Created on: Jul 18, 2023
 *      Author: elberia
 */


#include "header.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static uint8_t compare_versions ( const char * version1, const char * version2 );



//This should not use a buffer
uint8_t header_get_data(uint32_t address, header_t *pHeader) {
#if defined(HAL_ICACHE_MODULE_ENABLED)
  HAL_ICACHE_Disable();
#endif

  if (strcmp((char *)(address + HEADER_ADDRESS), BOOT_HEADER) != 0)
  {
    return HEADER_ERROR;
  }

  pHeader->pSize = (char *) (address + SIZE_ADDRESS);
  pHeader->pBoard = (char *) (address + BOARD_NAME_ADDRESS);
  pHeader->pRev = (char *) (address + REVISION_ADDRESS);

#if defined(HAL_ICACHE_MODULE_ENABLED)
  HAL_ICACHE_Enable();
#endif

  return HEADER_SUCCESS;
}



static uint8_t compare_versions ( const char * version1, const char * version2 ) {
    unsigned major1 = 0, minor1 = 0, bugfix1 = 0;
    unsigned major2 = 0, minor2 = 0, bugfix2 = 0;
    sscanf(version1, "%u.%u.%u", &major1, &minor1, &bugfix1);
    sscanf(version2, "%u.%u.%u", &major2, &minor2, &bugfix2);
    if (major1 < major2) return -1;
    if (major1 > major2) return 1;
    if (minor1 < minor2) return -1;
    if (minor1 > minor2) return 1;
    if (bugfix1 < bugfix2) return -1;
    if (bugfix1 > bugfix2) return 1;
    return 0;
}

uint8_t validate_header(header_t *pHeader) {

	if (strcmp(pHeader->pBoard, BOARD_NAME) != 0) {
		return HEADER_ERROR;
	}

	// Verify size is less than or equal to max
	if (atoi(pHeader->pSize) > MAX_HOTA_IMAGE_SIZE) {
		return HEADER_ERROR;
	}

	// Verify Version is greater that 0.0.0
	const char *version0 = "0.0.0";
	if (compare_versions(version0, pHeader->pRev) <= 0) {
		return HEADER_ERROR;
	}

	return HEADER_SUCCESS;
}


//This should take two header structures
uint8_t validate_ota(header_t *app1, header_t *app2) {

	// Verify OTA is for same board
	if(strcmp(app1->pBoard, app2->pBoard) != 0) {
		return HEADER_ERROR;
	}

	// Verify version of OTA is greater than
	if (compare_versions(app1->pRev, app2->pRev) <= 0) {
		return HEADER_ERROR;
	}

	// Verify size is less than or equal to max
	if (atoi(app2->pSize) > MAX_HOTA_IMAGE_SIZE) {
		return HEADER_ERROR;
	}

	return HEADER_SUCCESS;
}





