/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : motion_sensor_publish.c
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

/* Includes ------------------------------------------------------------------*/
/* Includes ------------------------------------------------------------------*/

#include "main.h"

/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"

#include "ExpressLink.h"

#include <stdio.h>

#include "lsm6dso.h"

#if USE_SENSOR
#include "custom_bus.h"
static LSM6DSO_Object_t LSM6DSO_Obj;
#endif
/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

#define ENV_SENSOR_MQTT_PUBLISH_TIME_BETWEEN_MS         ( 1000 )

/* 0: no debug, 1: Errors only, 2: INFO, 3: all (Error, Info, Debug)  */
#define DEBUG_LEVEL 1

#include "debug.h"
/* Private variables ---------------------------------------------------------*/
extern SemaphoreHandle_t ExpressLink_MutexHandle;
extern SemaphoreHandle_t SENSORS_I2C_MutexHandle;

#define BUFFER_SIZE_MOTION (EXPRESSLINK_TX_BUFFER_SIZE/4)

static char payloadBuf[BUFFER_SIZE_MOTION];
static char topic     [BUFFER_SIZE_MOTION];

static LSM6DSO_Axes_t MotionSensorsData;

/* Private function prototypes -----------------------------------------------*/
static int32_t Sensors_Init(void);
static void Sensors_GetData(LSM6DSO_Axes_t *SensorsData);

/* User code ---------------------------------------------------------*/
void vMotionSensorPublishTask(void *argument)
{
  char *response;

  PRINTF("[INFO] MotionSensorPublish started\r\n");

  /* Initialize the sensor */
  if (Sensors_Init() == LSM6DSO_ERROR)
  {
    PRINTF_ERROR("[ERROR] Motion Sensor Init failed\r\n");

    vTaskSuspend( NULL);
  }

  /* Acquire the express link mutex */
  xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);

  /* Get the thing name */
  response = ExpressLink_GetThingName();

  /* Format the topic <ThingName>/env_sensor_data */
  response[strlen(response) - 2] = '\0';
  snprintf(topic, BUFFER_SIZE_MOTION, "%s/motion_sensor_data", response);

  /* Set the topic */
  response = ExpressLink_SetTopic(TOPIC_INDEX_MOTION, topic);

  /* Release the express link mutex */
  xSemaphoreGive(ExpressLink_MutexHandle);

  for (;;)
  {
    /* Read Sensor data */
    Sensors_GetData(&MotionSensorsData);

    /* Format the message with the new sensor data */
    snprintf(payloadBuf,
    BUFFER_SIZE_MOTION, "{"
        "\"acceleration_mG\":"
        "{"
        "\"x\": %ld,"
        "\"y\": %ld,"
        "\"z\": %ld"
        "}"
        "}", MotionSensorsData.x, MotionSensorsData.y, MotionSensorsData.z);

    PRINTF_INFO("[INFO] Sending MQTT message: %s\r\n", payloadBuf);

    /* Acquire the express link mutex */
    xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
    /* Publish the sensor data message */
    taskENTER_CRITICAL();
    response = ExpressLink_SendMessage(TOPIC_INDEX_MOTION, payloadBuf);
    taskEXIT_CRITICAL();

    if (strcmp(response, "OK\r\n") != 0)
    {
      PRINTF_ERROR("\r\n[ERROR] ExpressLink_SendMessage. Generating reset\r\n");
      NVIC_SystemReset();
    }

    /* Release the express link mutex */
    xSemaphoreGive(ExpressLink_MutexHandle);

    vTaskDelay(pdMS_TO_TICKS(ENV_SENSOR_MQTT_PUBLISH_TIME_BETWEEN_MS));
  }
}

static int32_t Sensors_Init(void)
{
#if USE_SENSOR
  xSemaphoreTake(SENSORS_I2C_MutexHandle, portMAX_DELAY);
  uint8_t          LSM6DSO_Id;
  uint8_t          Status;
  LSM6DSO_IO_t     LSM6DSO_io_ctx = {0};

  /* Configure the accelerometer driver */
  LSM6DSO_io_ctx.BusType     = LSM6DSO_I2C_BUS; /* I2C */
  LSM6DSO_io_ctx.Address     = LSM6DSO_I2C_ADD_H;
  LSM6DSO_io_ctx.Init        = BSP_I2C1_Init;
  LSM6DSO_io_ctx.DeInit      = BSP_I2C1_DeInit;
  LSM6DSO_io_ctx.ReadReg     = BSP_I2C1_ReadReg;
  LSM6DSO_io_ctx.WriteReg    = BSP_I2C1_WriteReg;

  LSM6DSO_RegisterBusIO(&LSM6DSO_Obj, &LSM6DSO_io_ctx);
  LSM6DSO_Init         (&LSM6DSO_Obj);
  LSM6DSO_ReadID       (&LSM6DSO_Obj, &LSM6DSO_Id);

  if(LSM6DSO_Id != LSM6DSO_ID)
  {
    return LSM6DSO_ERROR;
  }

  LSM6DSO_ACC_Enable   (&LSM6DSO_Obj);

  do
  {
	  LSM6DSO_ACC_Get_DRDY_Status(&LSM6DSO_Obj, &Status);
  }while(Status != 1);

  xSemaphoreGive(SENSORS_I2C_MutexHandle);
#else
  MotionSensorsData.x=300;
  MotionSensorsData.y=400;
  MotionSensorsData.z=500;
#endif

  return LSM6DSO_OK;
}

static void Sensors_GetData(LSM6DSO_Axes_t *SensorsData)
{
#if USE_SENSOR
  xSemaphoreTake(SENSORS_I2C_MutexHandle, portMAX_DELAY);
  LSM6DSO_ACC_GetAxes(&LSM6DSO_Obj, SensorsData);
  xSemaphoreGive(SENSORS_I2C_MutexHandle);
#else
  SensorsData->x += 12;
  SensorsData->y += 24;
  SensorsData->z += 56;

  SensorsData->x %= 1000;
  SensorsData->y %= 1000;
  SensorsData->z %= 1000;
#endif
}

