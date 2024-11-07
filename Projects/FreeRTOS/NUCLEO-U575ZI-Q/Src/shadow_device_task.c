/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : aws_iot_demo_shadow.c
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ExpressLink.h"
#include "core_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "shadow_device_task.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  uint32_t ulCurrentPowerOnState; /* The device current power on state. */
  uint32_t ulReportedPowerOnState;/* The last reported state. It is initialized to an invalid value so that an update is initially sent. */
} ShadowDeviceCtx_t;

/* Private macro -------------------------------------------------------------*/
/* 0: no debug, 1: Errors only, 2: INFO, 3: all (Error, Info, Debug)  */
#define DEBUG_LEVEL 1

#include "debug.h"

#define MQTT_PUBLISH_MAX_LEN                 ( EXPRESSLINK_TX_BUFFER_SIZE )
#define MQTT_PUBLICH_TOPIC_STR_LEN           ( EXPRESSLINK_TX_BUFFER_SIZE )

#define shadowexampleSHADOW_REPORTED_JSON \
		"{"                                   \
		"\"state\":{"                         \
		"\"reported\":{"                      \
		"\"powerOn\": \"%1u\","               \
		"\"Board\" : \"%s\","                 \
		"\"Connectivity\" : \"%s\""           \
		"}"                                   \
		"}"                                   \
		"}"
#define shadowexampleSHADOW_REPORTED_JSON_LENGTH       (sizeof( shadowexampleSHADOW_REPORTED_JSON )+ sizeof(BOARD) + sizeof("Cellular"))

/* Private variables ---------------------------------------------------------*/
static char *response;

ShadowDeviceCtx_t xShadowCtx = { 0 };

extern SemaphoreHandle_t ExpressLink_MutexHandle;

uint32_t wifi = 0;

static void Publish_ShadowUpdate(void);
static void Handle_DeltaCallback(ShadowDeviceCtx_t *pxCtx, char *pxPublishInfo);

/* User code ---------------------------------------------------------*/
/**
 * @brief Publish AWS Shadow message
 * @retval None
 */
static void Publish_ShadowUpdate(void)
{
  if (xShadowCtx.ulCurrentPowerOnState != xShadowCtx.ulReportedPowerOnState)
  {
    char pcUpdateDocument[shadowexampleSHADOW_REPORTED_JSON_LENGTH + 1] =
      { 0 };

    /* Send shadow update */

    PRINTF_INFO("[INFO] Updating Shadow\r\n");

    xShadowCtx.ulReportedPowerOnState = xShadowCtx.ulCurrentPowerOnState;

    /* Build the JSON message */
    snprintf(pcUpdateDocument,
    shadowexampleSHADOW_REPORTED_JSON_LENGTH + 1,
    shadowexampleSHADOW_REPORTED_JSON,
	(unsigned int) xShadowCtx.ulCurrentPowerOnState,
	BOARD,
	wifi? "Wi-Fi":"Cellular");

    /* Send the message */
    xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
    response = ExpressLink_ShadowUpdate(0, pcUpdateDocument);

    if (strcmp(response, "OK\r\n") != 0)
    {
      PRINTF_ERROR("\r\n[%s] Publish_ShadowUpdate Generating reset\r\n", response);

      NVIC_SystemReset();
    }
    xSemaphoreGive(ExpressLink_MutexHandle);
  }
}

/**
 * @brief Handle AWS Shadow Delata message
 * @retval None
 */
void Handle_DeltaCallback(ShadowDeviceCtx_t *pxCtx, char *pxPublishInfo)
{
  static uint32_t ulCurrentVersion = 0; /* Remember the latest version number we've received */
  uint32_t ulVersion = 0UL;
  uint32_t ulNewState = 0UL;
  char *pcOutValue = NULL;
  uint32_t ulOutValueLength = 0UL;
  JSONStatus_t result = JSONSuccess;

  {
    /* Handle received delta shadow message */
    /* The payload will look similar to this:
     * {
     *      "state": {
     *          "powerOn": 1
     *      },
     *      "metadata": {
     *          "powerOn": {
     *              "timestamp": 1595437367
     *          }
     *      },
     *      "timestamp": 1595437367,
     *      "clientToken": "388062",
     *      "version": 12
     *  }
     */

    /* Make sure the payload is a valid json document. */
    result = JSON_Validate(pxPublishInfo, strlen(pxPublishInfo));

    if (result != JSONSuccess)
    {
      PRINTF_ERROR("[ERROR] Invalid JSON document received!\r\n");
    }
    else
    {
      /* Obtain the version value. */
      result = JSON_Search((char* ) pxPublishInfo, strlen(pxPublishInfo), "version", sizeof("version") - 1, &pcOutValue, (size_t* ) &ulOutValueLength);

      if (result != JSONSuccess)
      {
        PRINTF_ERROR("[ERROR] Version field not found in JSON document!\r\n");
      }
      else
      {
        /* Convert the extracted value to an unsigned integer value. */
        ulVersion = (uint32_t) strtoul(pcOutValue, NULL, 10);

        /* Make sure the version is newer than the last one we received. */
        if (ulVersion <= ulCurrentVersion)
        {
          /* In this demo, we discard the incoming message
           * if the version number is not newer than the latest
           * that we've received before. Your application may use a
           * different approach.
           */
          PRINTF_ERROR("[WARN] Received unexpected delta update with version %u. Current version is %u\r\n", (unsigned int) ulVersion, (unsigned int) ulCurrentVersion);
        }
        else
        {
          PRINTF_INFO("[INFO] Received delta update with version %.*s.\r\n", (int) ulOutValueLength, pcOutValue);
          /* Set received version as the current version. */
          ulCurrentVersion = ulVersion;

          /* Get powerOn state from json documents. */
          result = JSON_Search((char* ) pxPublishInfo, strlen(pxPublishInfo), "state.powerOn", sizeof("state.powerOn") - 1, &pcOutValue, (size_t* ) &ulOutValueLength);

          if (result != JSONSuccess)
          {
            PRINTF_ERROR("[WARN] powerOn field not found in JSON document!\r\n");
          }
          else
          {
            /* Convert the powerOn state value to an unsigned integer value. */
            ulNewState = (uint32_t) strtoul(pcOutValue, NULL, 10);

            PRINTF_INFO("[INFO] Setting powerOn state to %u.\r\n", (unsigned int) ulNewState);

            /* Set the new powerOn state. */
            pxCtx->ulCurrentPowerOnState = ulNewState;

            if (ulNewState == 1)
            {
              HAL_GPIO_WritePin( LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); /* Turn the LED ON */
            }
            else
            {
              HAL_GPIO_WritePin( LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); /* Turn the LED off */
            }
          }
        }
      }
    }
  }
}

/**
 * @brief The AWS Shadow demo
 * @retval None
 */
void vShadowDemoTask(void *argument)
{
  /* USER CODE BEGIN vShadowDemoTask */
  uint32_t clientToken = 12345678;
  char *response;
  int shadow_event = 0;

  xShadowCtx.ulCurrentPowerOnState = HAL_GPIO_ReadPin(LD2_GPIO_Port, LD2_Pin);
  xShadowCtx.ulReportedPowerOnState = !xShadowCtx.ulCurrentPowerOnState;

  PRINTF("[INFO] ShadowDemo Task started\r\n");

  /* Enable Shadow service*/
  xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
  if (strstr(ExpressLink_GetSSID(), "OK") != NULL) /* Is this a Wi-Fi module? */
  {
	  wifi = 1;
  }

  ExpressLink_ShadowEnable();
  ExpressLink_ShadowSetToken(clientToken);
  ExpressLink_ShadowInit(0);
  ExpressLink_ShadowSubscribe(0);
  xSemaphoreGive(ExpressLink_MutexHandle);

  /* Infinite loop */
  for (;;)
  {
    /* Wait for shadow event */
    shadow_event = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    switch (shadow_event)
    {
    case EXPRESSLINK_EVENT_SHADOW_INIT:
      PRINTF_INFO("[INFO] Shadow interface was initialized successfully.\r\n");

      /* Send a shadow update */
      Publish_ShadowUpdate();
      break;

    case EXPRESSLINK_EVENT_SHADOW_INIT_FAILED:
      PRINTF_INFO("[INFO] The SHADOW interface initialization failed.\r\n");
      break;

    case EXPRESSLINK_EVENT_SHADOW_INIT_DOC:
      PRINTF_INFO("[INFO] A Shadow document was received.\r\n");

      xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
      taskENTER_CRITICAL(); /* We need to suspend interrupts to not disturb USART communication and risk to corrupt received messages */
      response = ExpressLink_ShadowGetDoc(0); /* Read the shadow document */
      taskEXIT_CRITICAL();

      PRINTF_INFO("%s",response);

      xSemaphoreGive(ExpressLink_MutexHandle);
      break;

    case EXPRESSLINK_EVENT_SHADOW_UPDATE:

      xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
      taskENTER_CRITICAL(); /* We need to suspend interrupts to not disturb USART communication and risk to corrupt received messages */
      response = ExpressLink_ShadowGetUpdate(0); /* Fetch the update message */
      taskEXIT_CRITICAL();

      PRINTF_INFO("[INFO] A Shadow update result was received: %s", response);

      xSemaphoreGive(ExpressLink_MutexHandle);

      break;

    case EXPRESSLINK_EVENT_SHADOW_DELTA:
      PRINTF_INFO("[INFO] A Shadow delta update was received.\r\n");

      xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
      taskENTER_CRITICAL(); /* We need to suspend interrupts to not disturb USART communication and risk to corrupt received messages */
      response = ExpressLink_ShadowGetDelta(0); /* Fetch the delta message */
      taskEXIT_CRITICAL();

      PRINTF_INFO("%s",response);

      /* Handle received delta shadow message */
      Handle_DeltaCallback(&xShadowCtx, &response[3]);

      xSemaphoreGive(ExpressLink_MutexHandle);

      /* Send a shadow update */
      Publish_ShadowUpdate();

      break;

    case EXPRESSLINK_EVENT_SHADOW_DELETE:
      PRINTF_INFO("[INFO] A Shadow delete result was received.\r\n");

      xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
      taskENTER_CRITICAL(); /* We need to suspend interrupts to not disturb USART communication and risk to corrupt received messages */
      response = ExpressLink_ShadowGetDelete(0); /* Fetch the delete message */
      taskEXIT_CRITICAL();

      PRINTF_INFO("%s",response);

      xSemaphoreGive(ExpressLink_MutexHandle);
      break;

    case EXPRESSLINK_EVENT_SHADOW_SUBACK:
      PRINTF_INFO("[INFO] A Shadow delta subscription was accepted.\r\n");
      break;

    case EXPRESSLINK_EVENT_SHADOW_SUBNACK:
      PRINTF_INFO("[INFO] A Shadow  delta subscription was rejected.\r\n");
      break;
    }
  }
  /* USER CODE END vShadowDemoTask */
}

