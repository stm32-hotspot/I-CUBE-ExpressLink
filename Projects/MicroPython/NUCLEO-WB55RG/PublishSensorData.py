# https://github.com/jposada202020/MicroPython_HTS221

import time
import json
from aws_iot_expresslink import ExpressLink
from machine import I2C, UART
from micropython_hts221 import hts221

i2c = I2C(1)
hts = hts221.HTS221(i2c)

expresslink_uart = UART(2, 115200)

el = ExpressLink(uart=expresslink_uart)

el.config.SSID = "st_iot_demo"
el.config.Passphrase = "stm32u585"
el.config.Endpoint = "a1qwhobjtvew8t-ats.iot.us-west-1.amazonaws.com"
el.config.CustomName = "STM32-EL-MX"

el.connect(non_blocking=True)

time.sleep(1)

while(el.connected==False):
      time.sleep(1)

el.config.set_topic(1, el.config.ThingName)

while True:
    j = {
        "temp_0_c": hts.temperature,
        "rh_pct": hts.relative_humidity
    }
    el.publish(topic_index=1, message=json.dumps(j))
    time.sleep(5)
