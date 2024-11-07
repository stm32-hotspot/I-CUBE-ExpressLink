/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : env_sensor_publish.c
 * @brief          : Demo publishing temperature and Humidity sensor data
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

#include "hts221.h"

#if USE_SENSOR
#include "custom_bus.h"
static HTS221_Object_t HTS221_Obj;
#endif
/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  float HTS221_HUM_Value;
  float HTS221_TEMP_Value;
} EnvSensorData_t;

/* Private macro -------------------------------------------------------------*/

#define ENV_SENSOR_MQTT_PUBLISH_TIME_BETWEEN_MS         ( 3000 )

/* 0: no debug, 1: Errors only, 2: INFO, 3: all (Error, Info, Debug)  */
#define DEBUG_LEVEL 1

#include "debug.h"
/* Private variables ---------------------------------------------------------*/
extern SemaphoreHandle_t ExpressLink_MutexHandle;
extern SemaphoreHandle_t SENSORS_I2C_MutexHandle;

#define BUFFER_SIZE_ENV (EXPRESSLINK_TX_BUFFER_SIZE/4)

static char payloadBuf[BUFFER_SIZE_ENV];
static char topic     [BUFFER_SIZE_ENV];

static EnvSensorData_t EnvSensorsData;

/* Private function prototypes -----------------------------------------------*/
static int32_t Sensors_Init(void);
static void Sensors_GetEnvData(EnvSensorData_t *SensorsData);

/* User code ---------------------------------------------------------*/
void vEnvSensorPublishTask(void *argument)
{
  char *response;

  PRINTF("[INFO] EnvSensorPublish started\r\n");

  /* Initialize the sensor */
  if(Sensors_Init() == HTS221_ERROR)
  {
    PRINTF_ERROR("[ERROR] ENV Sensor Init failed\r\n");

    vTaskSuspend( NULL);
  }

  /* Acquire the express link mutex */
  xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);

  /* Get the thing name */
  response = ExpressLink_GetThingName();

  /* Format the topic <ThingName>/env_sensor_data */
  response[strlen(response) - 2] = '\0';
  snprintf(topic, BUFFER_SIZE_ENV, "%s/env_sensor_data", response);

  /* Set the topic */
  response = ExpressLink_SetTopic(TOPIC_INDEX_ENV, topic);

  /* Release the express link mutex */
  xSemaphoreGive(ExpressLink_MutexHandle);

  for (;;)
  {
    /* Read Sensor data */
    Sensors_GetEnvData(&EnvSensorsData);

    /* Format the message with the new sensor data */
    snprintf(payloadBuf, BUFFER_SIZE_ENV, "{\"temp_0_c\": %d, \"rh_pct\": %d}", (int) EnvSensorsData.HTS221_TEMP_Value, (int) EnvSensorsData.HTS221_HUM_Value);

    PRINTF_INFO("[INFO] Sending MQTT message: %s\r\n", payloadBuf);

    /* Acquire the express link mutex */
    xSemaphoreTake(ExpressLink_MutexHandle, portMAX_DELAY);
    /* Publish the sensor data message */
    taskENTER_CRITICAL();
    response = ExpressLink_SendMessage(TOPIC_INDEX_ENV, payloadBuf);
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

  uint8_t          HTS221_Id;
  uint8_t          Status;
  HTS221_IO_t      HTS221_io_ctx = {0};

  /* Configure the accelerometer driver */
  HTS221_io_ctx.BusType     = HTS221_I2C_BUS; /* I2C */
  HTS221_io_ctx.Address     = HTS221_I2C_ADDRESS;
  HTS221_io_ctx.Init        = BSP_I2C1_Init;
  HTS221_io_ctx.DeInit      = BSP_I2C1_DeInit;
  HTS221_io_ctx.ReadReg     = BSP_I2C1_ReadReg;
  HTS221_io_ctx.WriteReg    = BSP_I2C1_WriteReg;

  HTS221_RegisterBusIO(&HTS221_Obj, &HTS221_io_ctx);
  HTS221_Init         (&HTS221_Obj);
  HTS221_ReadID       (&HTS221_Obj, &HTS221_Id);

  if(HTS221_Id != HTS221_ID)
  {
    return HTS221_ERROR;
  }


  HTS221_HUM_Enable   (&HTS221_Obj);

  do
  {
    HTS221_HUM_Get_DRDY_Status(&HTS221_Obj, &Status);
  }while(Status != 1);

  do
  {
    HTS221_TEMP_Get_DRDY_Status(&HTS221_Obj, &Status);
  }while(Status != 1);

  xSemaphoreGive(SENSORS_I2C_MutexHandle);
#else

  EnvSensorsData.HTS221_HUM_Value = 50;
  EnvSensorsData.HTS221_TEMP_Value = 30;
#endif

  return HTS221_OK;
}

static void Sensors_GetEnvData(EnvSensorData_t *SensorsData)
{
#if USE_SENSOR
  xSemaphoreTake(SENSORS_I2C_MutexHandle, portMAX_DELAY);
  HTS221_HUM_GetHumidity    (&HTS221_Obj, &SensorsData->HTS221_HUM_Value);
  HTS221_TEMP_GetTemperature(&HTS221_Obj, &SensorsData->HTS221_TEMP_Value);
  xSemaphoreGive(SENSORS_I2C_MutexHandle);
#else
  SensorsData->HTS221_HUM_Value += 5;
  SensorsData->HTS221_TEMP_Value += 3;

  if (SensorsData->HTS221_HUM_Value > 70)
  {
    SensorsData->HTS221_HUM_Value = 20;
  }

  if (SensorsData->HTS221_TEMP_Value > 45)
  {
    SensorsData->HTS221_TEMP_Value = 5;
  }
#endif
}

