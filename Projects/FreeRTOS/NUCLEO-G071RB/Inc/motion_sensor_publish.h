/* USER CODE BEGIN Header */
/**
******************************************************************************
* @file           : motion_sensor_publish.h
* @brief          : ExpressLink motion_sensor_publish
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
#ifndef __MOTION_SENSOR_PUBLISH_H__
#define __MOTION_SENSOR_PUBLISH_H__

#ifdef __cplusplus
extern "C" {
#endif
  
/* Includes ------------------------------------------------------------------*/
  
/* Exported types ------------------------------------------------------------*/


/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

  
/* Exported functions prototypes ---------------------------------------------*/

void vMotionSensorPublishTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __MOTION_SENSOR_PUBLISH_H__ */
