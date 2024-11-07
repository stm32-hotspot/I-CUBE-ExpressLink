/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : PubSub.c
 * @brief          : ExpressLink Publish and subscribe demo
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

#include <PubSub.h>
#include "main.h"

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include "ExpressLink.h"

#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
#define PUB_SUB_MQTT_PUBLISH_TIME_BETWEEN_MS         ( 5000 )

/* 0: no debug, 1: Errors only, 2: INFO, 3: all (Error, Info, Debug)  */
#define DEBUG_LEVEL 1

#include "debug.h"
/* Private variables ---------------------------------------------------------*/
extern SemaphoreHandle_t ExpressLink_MutexHandle;
extern EventGroupHandle_t ExpressLink_EventHandle;

extern uint32_t msg_received;
extern uint32_t msg_sent;

static char payloadBuf[EXPRESSLINK_RX_BUFFER_SIZE];
static char topic[EXPRESSLINK_RX_BUFFER_SIZE];

static MQTTPublishInfo_t msg;

/* User code ---------------------------------------------------------*/
void vPubSubTask(void *argument)
{
  uint32_t n = 0;

  msg.pTopic = topic;
  msg.pPayload = payloadBuf;
  char *response;

#if (DEBUG_LEVEL > 0)
  setvbuf(stdout, NULL, _IONBF, 0); /* Disable buffering for stdout */
#endif

  PRINTF("[INFO] PubSub Task started\r\n");

  xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
  ExpressLink_SetTopic(TOPIC_INDEX_PubSub, ExpressLink_GetThingName());
  ExpressLink_SubscribeToTopic(TOPIC_INDEX_PubSub); /* Use the ThingName as a topic to publish and subscribe to */
  xSemaphoreGive(ExpressLink_MutexHandle);

  xEventGroupWaitBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_SUBACK, pdTRUE, pdTRUE, portMAX_DELAY);

  for (;;)
  {
    memset(payloadBuf, 0, EXPRESSLINK_TX_BUFFER_SIZE);

    snprintf(payloadBuf, EXPRESSLINK_TX_BUFFER_SIZE, "{\"Hello world\": %lu}", n++);

    PRINTF_INFO("[INFO] Sending MQTT message: %s\r", payloadBuf);

    vTaskDelay(pdMS_TO_TICKS(PUB_SUB_MQTT_PUBLISH_TIME_BETWEEN_MS));

    xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
    response = ExpressLink_SendMessage(TOPIC_INDEX_PubSub, payloadBuf); /* Send a message */

    if (strcmp(response, "OK\r\n") != 0)
    {
      PRINTF_ERROR("\r\n[ERROR] PubSub Generating reset: %s\r\n", response);

      NVIC_SystemReset();
    }

    xSemaphoreGive(ExpressLink_MutexHandle);

    /* Wait for a message */
    xEventGroupWaitBits(ExpressLink_EventHandle, EXPRESSLINK_EVENT_MESSAGE, pdTRUE, pdTRUE, portMAX_DELAY);

    memset(payloadBuf, 0, EXPRESSLINK_RX_BUFFER_SIZE);

    xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
    taskENTER_CRITICAL(); /* We need to suspend interrupts to not disturb USART communication and risk to corrupt received messages */
    ExpressLink_GetMessage(&msg);
    taskEXIT_CRITICAL();
    xSemaphoreGive(ExpressLink_MutexHandle);

    if (msg.payloadLength > 0)
    {
      PRINTF_INFO("[INFO] Message received on Topic : %s[INFO] Message                   : %s", msg.pTopic, msg.pPayload);
    }
  }
}
