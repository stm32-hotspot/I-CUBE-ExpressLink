/**
  ******************************************************************************
  * File Name          : STMicroelectronics.I-CUBE-STDIO_conf.h
  * Description        : This file provides code for the configuration
  *                      of the STMicroelectronics.I-CUBE-STDIO_conf.h instances.
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STMICROELECTRONICS__I_CUBE_STDIO_CONF__H__
#define __STMICROELECTRONICS__I_CUBE_STDIO_CONF__H__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/**
	MiddleWare name : STMicroelectronics.I-CUBE-STDIO.1.5.0
	MiddleWare fileName : STMicroelectronics.I-CUBE-STDIO_conf.h
	MiddleWare version :
*/
/*---------- STDIN_UART_HANDLER  -----------*/
#define STDIN_UART_HANDLER      huart1

/*---------- STDIN_USBD_FS_HANDLER  -----------*/
#define STDIN_USBD_FS_HANDLER      hUsbDeviceFS

/*---------- STDIN_USBD_HS_HANDLER  -----------*/
#define STDIN_USBD_HS_HANDLER      hUsbDeviceHS

/*---------- STDIN_USBH_FS_HANDLER  -----------*/
#define STDIN_USBH_FS_HANDLER      hUsbHostFS

/*---------- STDIN_USBH_HS_HANDLER  -----------*/
#define STDIN_USBH_HS_HANDLER      hUsbHostHS

/*---------- STDOUT_UART_HANDLER  -----------*/
#define STDOUT_UART_HANDLER      huart2

/*---------- STDOUT_USBD_FS_HANDLER  -----------*/
#define STDOUT_USBD_FS_HANDLER      hUsbDeviceFS

/*---------- STDOUT_USBD_HS_HANDLER  -----------*/
#define STDOUT_USBD_HS_HANDLER      hUsbDeviceHS

#ifdef __cplusplus
}
#endif
#endif /*__ STMICROELECTRONICS__I_CUBE_STDIO_CONF__H_H */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
