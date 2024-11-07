/**
******************************************************************************
* @file           : PublishSensorData.ino 
* @board          : B-U585I-IOT02A
* @brief          : Example for the ExpressLink send Hum and temp data
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

// https://github.com/awslabs/aws-iot-expresslink-library-arduino
// https://github.com/stm32duino/HTS221

#include <ExpressLink.h>
#include <HTS221Sensor.h>
#include <string.h>

#define EXPRESSLINK_SERIAL_RX_PIN   0
#define EXPRESSLINK_SERIAL_TX_PIN   1

// B-U585I-IOT02A
#define I2C2_SCL    PH4
#define I2C2_SDA    PH5

#define MY_AWS_IOT_ENDPOINT "a1qwhobjtvew8t.iot.us-west-1.amazonaws.com"
#define MY_SSID             "st_iot_demo"
#define MY_Passphrase       "stm32u585"

#define stlink_serial      Serial

HardwareSerial expresslink_serial(EXPRESSLINK_SERIAL_RX_PIN, EXPRESSLINK_SERIAL_TX_PIN);

// I2C
TwoWire dev_i2c(I2C2_SDA, I2C2_SCL);

// Components.
HTS221Sensor  HumTemp(&dev_i2c);

ExpressLink el;

void getSensorData(float *humidity, float *temperature)
{
  HumTemp.GetHumidity(humidity);
  HumTemp.GetTemperature(temperature);
}

void setup()
{
  delay(5000);

  stlink_serial.begin(115200);
  
  // Initialize I2C bus.
  dev_i2c.begin();

  // Initlialize components.
  HumTemp.begin();
  HumTemp.Enable();

  stlink_serial.println("Initializing ExpressLink.");

  expresslink_serial.begin(ExpressLink::BAUDRATE);

  if (!el.begin(expresslink_serial, 2, -1, -1, false))
  {
    stlink_serial.println("Failed to initialize ExpressLink.");
    while (1);
  }

  String ThigName = el.config.getThingName();
  stlink_serial.print("ThigName : ");
  stlink_serial.print(ThigName);
  stlink_serial.println("");

  String topic1 = ThigName + "/env_sensor_data";

  el.config.setTopic(1, topic1);
  el.config.setSSID(MY_SSID);
  el.config.setPassphrase(MY_Passphrase);
  el.config.setEndpoint(MY_AWS_IOT_ENDPOINT);
  el.config.setCustomName("STM32-EL-MX");
  
  if(!el.connect(false))/* Connect in synchronous mode */
  {
    stlink_serial.println("Failed to connect to AWS");
    while(1);
  }

  Serial.println("Connected to AWS");
}

void loop()
{
  float humidity;
  float temperature;

  getSensorData(&humidity, &temperature);

  String json = "{\"temperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + "}";
  stlink_serial.println(json);

  el.send(1, json);

  delay(3000);
}