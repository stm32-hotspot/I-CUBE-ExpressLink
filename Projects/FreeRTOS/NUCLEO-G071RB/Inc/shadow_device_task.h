/* USER CODE BEGIN Header */
/**
******************************************************************************
* @file           : aws_iot_demo_shadow.h
* @brief          : ExpressLink aws_iot_demo_shadow
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
#ifndef __AWS_IOT_DEMO_SHADOW_H
#define __AWS_IOT_DEMO_SHADOW_H

#ifdef __cplusplus
extern "C" {
#endif
  
/* Includes ------------------------------------------------------------------*/
  
/* Exported types ------------------------------------------------------------*/


/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

  
/* Exported functions prototypes ---------------------------------------------*/

void vShadowDemoTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __AWS_IOT_DEMO_SHADOW_H */