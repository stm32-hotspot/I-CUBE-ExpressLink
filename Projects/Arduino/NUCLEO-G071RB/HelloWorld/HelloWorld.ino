/**
******************************************************************************
* @file           : HelloWorld.ino 
* @brief          : Hello World Example for the ExpressLink
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
#include <ExpressLink.h>

#define EXPRESSLINK_SERIAL_RX_PIN   0
#define EXPRESSLINK_SERIAL_TX_PIN   1

#define MY_AWS_IOT_ENDPOINT "a1qwhobjtvew8t.iot.us-west-1.amazonaws.com"
#define MY_SSID             "st_iot_demo"
#define MY_Passphrase       "stm32u585"

#define stlink_serial      Serial

HardwareSerial expresslink_serial(EXPRESSLINK_SERIAL_RX_PIN, EXPRESSLINK_SERIAL_TX_PIN);

ExpressLink el;

void setup()
{
  delay(5000);

  stlink_serial.begin(115200);
  
  stlink_serial.println("Initializing ExpressLink.");

  expresslink_serial.begin(ExpressLink::BAUDRATE);

  if (!el.begin(expresslink_serial, 2, -1, -1, false))
  {
    stlink_serial.println("Failed to initialize ExpressLink.");
    while (1);
  }

  String ThigName = el.config.getThingName();

  String topic1 = ThigName;

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
  String msg = "Hello world";
  
  stlink_serial.println(msg);

  el.send(1, msg);

  delay(3000);
}
