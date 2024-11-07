/**
 ******************************************************************************
 * @file           : stdio_uart.c
 * @version        : v 1.0.0
 * @brief          : This file implements stdin USART channel
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * ththe "License"; You may not use this file except in compliance with the
 *                             opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

#include "main.h"
#include "STMicroelectronics.I-CUBE-STDIO_conf.h"
#include <stdio.h>

#ifndef __ICCARM__ /* If not IAR */
int fgetc(FILE *stream);
int _read(int file, char *ptr, int len);
#endif

extern UART_HandleTypeDef STDIN_UART_HANDLER;

void stdin_init(void);

void stdin_init(void)
{
  /* This was intentionally left empty */
}

#ifdef __ICCARM__
int __read(int file, char *ptr, int len)
#else
int _read(int file, char *ptr, int len)
#endif
{
  int length = 0;

  int ch = 0;
  do
  {
    HAL_UART_Receive(&STDIN_UART_HANDLER, (uint8_t*) &ch, 1, 0xFFFFFFFF);

    *ptr = ch;
    ptr++;
    length++;
  } while ((length < len) && (*(ptr - 1) != '\r'));

  return length;
}

#ifdef __ARMCC_VERSION /* Keil Compiler */
int fgetc(FILE * stream)
{
  char ch;
  _read(0, &ch, 1);
  
  return (int)ch;
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
