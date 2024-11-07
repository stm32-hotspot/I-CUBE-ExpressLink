import time
import json
from pyb import UART
from aws_iot_expresslink import ExpressLink

expresslink_uart = UART(4, 115200)

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
el.subscribe       (1, el.config.ThingName)

while True:
    j = {
        "message": f"Hello World from {el.config.ThingName}!",
    }
    el.publish(topic_index=1, message=json.dumps(j))
    time.sleep(5)
    topic_name, message = el.get_message(1)
    print("Message received : " + message)
    print("Topic            : " + topic_name)
