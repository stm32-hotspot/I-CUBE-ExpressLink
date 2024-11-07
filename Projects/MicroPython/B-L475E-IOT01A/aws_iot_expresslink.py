"""
AWS IoT ExpressLink - Python library
Version: 1.0.0 from 2023-04-03
Source: https://github.com/awslabs/aws-iot-expresslink-library-python
License: MIT-0
"""

import time
import re
import sys
from collections import namedtuple

IS_CIRCUITPYTON = sys.implementation.name == "circuitpython"
IS_MICROPYTON = sys.implementation.name == "micropython"

try:
    from typing import Optional, Tuple, Union  # pylint: disable=unused-import
except ImportError:
    # CircuitPython 7 does not support the typing module.
    pass

try:
    from enum import Enum
except ImportError:
    # To keep compatibility between Python interpreters, simply use generic object as wrapper.
    Enum = object

if IS_CIRCUITPYTON:
    import busio
    import digitalio
    import microcontroller
    import usb_cdc
    from adafruit_debouncer import Debouncer
elif IS_MICROPYTON:
    import machine
else:
    # ... try pySerial on Linux/macOS/Windows
    import serial


def _escape(s: str) -> str:
    # escaping https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-delimiters
    # use r"" raw strings if needed as input
    s = s.replace("\\", "\\\\")
    s = s.replace("\r", "\\D")
    s = s.replace("\n", "\\A")
    return s


def _unescape(s: str) -> str:
    # escaping https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-delimiters
    # use r"" raw strings if needed as input
    s = s.replace("\\A", "\n")
    s = s.replace("\\D", "\r")
    s = s.replace("\\\\", "\\")
    return s


def _readline(uart, debug=False, delay=True) -> str:
    if delay:
        time.sleep(
            0.1
        )  # give it a bit of time to accumulate data - it might crash or loose bytes without it!

    l = b""
    counter = 300  # or ExpressLink.TIMEOUT
    for _ in range(counter):
        p = uart.readline()
        if p:
            l += p
        if l.endswith(b"\n"):
            break
    else:
        print("Expresslink UART timeout - response might be incomplete.")

    l = l.decode().strip("\r\n\x00\xff\xfe\xfd\xfc\xfb\xfa")
    l = _unescape(l)

    if debug:
        print("< " + l)
    return l


def _init_input_pin(pin: Union["microcontroller.Pin", "machine.Pin"]):
    if not pin:
        return None

    if IS_CIRCUITPYTON:
        s = digitalio.DigitalInOut(pin)
        s.direction = digitalio.Direction.INPUT
        s.pull = digitalio.Pull.UP
        return Debouncer(s, interval=0.001)  # to get a useful rose/fell flag
    elif IS_MICROPYTON:
        pin.init(mode=machine.Pin.IN, pull=machine.Pin.PULL_UP)
        return pin
    else:
        raise RuntimeError("unknown platform")


def _init_output_pin(pin: Union["microcontroller.Pin", "machine.Pin"]):
    if not pin:
        return None

    if IS_CIRCUITPYTON:
        s = digitalio.DigitalInOut(pin)
        s.direction = digitalio.Direction.OUTPUT
        return s
    elif IS_MICROPYTON:
        pin.init(mode=machine.Pin.OUT)
        return pin
    else:
        raise RuntimeError("unknown platform")


def _is_pin_high(pin: Union["microcontroller.Pin", "machine.Pin"]):
    if not pin:
        return False

    if IS_CIRCUITPYTON:
        return pin.value == True
    elif IS_MICROPYTON:
        return pin.value()
    else:
        raise RuntimeError("unknown platform")


def _set_pin(pin: Union["microcontroller.Pin", "machine.Pin"], value: bool):
    if IS_CIRCUITPYTON:
        pin.value = True
    elif IS_MICROPYTON:
        pin.value(value)
    else:
        raise RuntimeError("unknown platform")


def _init_uart(*, uart, rx, tx, uart_id, port):
    if uart:
        return uart
    elif rx and tx:
        if IS_CIRCUITPYTON:
            return busio.UART(
                tx=tx,
                rx=rx,
                baudrate=ExpressLink.BAUDRATE,
                timeout=ExpressLink.TIMEOUT,
                receiver_buffer_size=ExpressLink.RX_BUFFER,
            )
        elif IS_MICROPYTON:
            return machine.UART(
                uart_id,
                tx=tx,
                rx=rx,
                baudrate=ExpressLink.BAUDRATE,
                timeout=int(ExpressLink.TIMEOUT * 1000),
                rxbuf=ExpressLink.RX_BUFFER,
            )
    elif port:
        return serial.Serial(
            port=port,
            baudrate=ExpressLink.BAUDRATE,
            timeout=ExpressLink.TIMEOUT,
        )
    raise RuntimeError("unknown platform")


class Event(Enum):
    """Event codes defined in https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-event-handling.html#elpg-event-handling-commands"""

    MSG = 1
    """parameter = topic index. A message was received on topic #."""
    STARTUP = 2
    """parameter = 0. The module has entered the active state."""
    CONLOST = 3
    """parameter = 0. Connection unexpectedly lost."""
    OVERRUN = 4
    """parameter = 0. Receive buffer Overrun (topic in detail)."""
    OTA = 5
    """parameter = 0. OTA event (see OTA? command for details)."""
    CONNECT = 6
    """parameter = Connection Hint. Connection established (== 0) or failed (> 0)."""
    CONFMODE = 7
    """parameter = 0. CONFMODE exit with success."""
    SUBACK = 8
    """parameter = Topic Index. Subscription accepted."""
    SUBNACK = 9
    """parameter = Topic Index. Subscription rejected."""
    # 10..19 RESERVED
    SHADOW_INIT = 20
    """parameter = Shadow Index. Shadow initialization successfully."""
    SHADOW_INIT_FAILED = 21
    """parameter = Shadow Index. Shadow initialization failed."""
    SHADOW_DOC = 22
    """parameter = Shadow Index. Shadow document received."""
    SHADOW_UPDATE = 23
    """parameter = Shadow Index. Shadow update result received."""
    SHADOW_DELTA = 24
    """parameter = Shadow Index. Shadow delta update received."""
    SHADOW_DELETE = 25
    """parameter = Shadow Index. Shadow delete result received"""
    SHADOW_SUBACK = 26
    """parameter = Shadow Index. Shadow delta subscription accepted."""
    SHADOW_SUBNACK = 27
    """parameter = Shadow Index. Shadow delta subscription rejected."""
    # <= 999 RESERVED


class OTACodes(Enum):
    """OTA Code defined in https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-ota-updates.html#elpg-ota-commands"""

    NoOTAInProgress = 0
    """ No OTA in progress."""
    UpdateProposed = 1
    """ A new module OTA update is being proposed. The host can inspect the version number and decide to accept or reject it. The {detail} field provides the version information (string)."""
    HostUpdateProposed = 2
    """ A new Host OTA update is being proposed. The host can inspect the version details and decide to accept or reject it. The {detail} field provides the metadata that is entered by the operator (string)."""
    OTAInProgress = 3
    """ OTA in progress. The download and signature verification steps have not been completed yet."""
    NewExpressLinkImageReady = 4
    """ A new module firmware image has arrived. The signature has been verified and the ExpressLink module is ready to reboot. (Also, an event was generated.)"""
    NewHostImageReady = 5
    """ A new host image has arrived. The signature has been verified and the ExpressLink module is ready to read its contents to the host. The size of the file is indicated in the response detail. (Also, an event was generated.)"""


class Config:
    def __init__(self, el: "ExpressLink") -> None:
        self._el = el
        self._topics = {}

    def _extract_value(self, query):
        success, line, error_code = self._el.cmd(f"CONF? {query}")
        if success:
            return line
        else:
            raise RuntimeError(f"failed to get config {query}: ERR{error_code} {line}")

    def _set_value(self, key, value):
        success, line, error_code = self._el.cmd(f"CONF {key}={value}")
        if success:
            return line
        else:
            raise RuntimeError(
                f"failed to set config {key}={value}: ERR{error_code} {line}"
            )

    @property
    def About(self) -> str:
        return self._extract_value("About")

    @property
    def Version(self) -> str:
        return self._extract_value("Version")

    @property
    def TechSpec(self) -> str:
        return self._extract_value("TechSpec")

    @property
    def ThingName(self) -> str:
        return self._extract_value("ThingName")

    @property
    def Certificate(self) -> str:
        return self._extract_value("Certificate pem").lstrip("pem").strip()

    @property
    def CustomName(self) -> str:
        return self._extract_value("CustomName")

    @CustomName.setter
    def CustomName(self, value: str) -> str:
        return self._set_value("CustomName", value)

    @property
    def Endpoint(self) -> str:
         return self._extract_value("Endpoint")

    @Endpoint.setter
    def Endpoint(self, value: str):
        return self._set_value("Endpoint", value)

    @property
    def RootCA(self) -> str:
        return self._extract_value("RootCA pem").lstrip("pem").strip()

    @RootCA.setter
    def RootCA(self, value: str):
        return self._set_value("RootCA", value)

    @property
    def DefenderPeriod(self) -> int:
        return int(self._extract_value("DefenderPeriod"))

    @DefenderPeriod.setter
    def DefenderPeriod(self, value: int):
        return self._set_value("DefenderPeriod", str(value))

    @property
    def HOTAcertificate(self) -> str:
        return self._extract_value("HOTAcertificate pem").lstrip("pem").strip()

    @HOTAcertificate.setter
    def HOTAcertificate(self, value: str):
        return self._set_value("HOTAcertificate", value)

    @property
    def OTAcertificate(self) -> str:
        return self._extract_value("OTAcertificate pem").lstrip("pem").strip()

    @OTAcertificate.setter
    def OTAcertificate(self, value: str):
        return self._set_value("OTAcertificate", value)

    @property
    def SSID(self) -> str:
        return self._extract_value("SSID")

    @SSID.setter
    def SSID(self, value: str):
        return self._set_value("SSID", value)

    @property
    def Passphrase(self):
        raise RuntimeError("write-only persistent key")

    @Passphrase.setter
    def Passphrase(self, value: str):
        return self._set_value("Passphrase", value)

    @property
    def APN(self) -> str:
        return self._extract_value("APN")

    @APN.setter
    def APN(self, value: str):
        return self._set_value("APN", value)

    @property
    def QoS(self) -> int:
        return int(self._extract_value("QoS"))

    @QoS.setter
    def QoS(self, value: int):
        return self._set_value("QoS", str(value))

    def get_topic(self, topic_index):
        topic_name = self._extract_value(f"Topic{topic_index}")
        self._topics[topic_index] = topic_name
        return topic_name

    def set_topic(self, topic_index, topic_name):
        topic_name = topic_name.strip()
        self._topics[topic_index] = topic_name
        return self._set_value(f"Topic{topic_index}", topic_name)

    @property
    def topics(self):
        return self._topics.copy()

    @property
    def EnableShadow(self) -> bool:
        return bool(self._extract_value("EnableShadow"))

    @EnableShadow.setter
    def EnableShadow(self, value: Union[bool, int]):
        return self._set_value(
            "EnableShadow", "1" if bool(value) else "0"
        )  # accept bool as well as int as input

    def get_shadow(self, shadow_index):
        return self._extract_value(f"Shadow{shadow_index}")

    def set_shadow(self, shadow_index, shadow_name):
        return self._set_value(f"Shadow{shadow_index}", shadow_name)

    @property
    def ShadowToken(self) -> str:
        return self._extract_value("ShadowToken")

    @ShadowToken.setter
    def ShadowToken(self, value: str):
        return self._set_value("ShadowToken", value)


class ExpressLink:
    BAUDRATE = 115200

    TIMEOUT = 0.1  # CircuitPython has a maxium of 100 seconds.

    RX_BUFFER = 4096

    def __init__(
        self,
        uart: Optional[Union["busio.UART", "machine.UART", "serial.Serial"]] = None,
        rx: Optional[Union["microcontroller.Pin", "machine.Pin"]] = None,
        tx: Optional[Union["microcontroller.Pin", "machine.Pin"]] = None,
        uart_id: Optional[int] = 0,
        port: Optional[str] = None,
        event_pin: Optional[Union["microcontroller.Pin", "machine.Pin"]] = None,
        wake_pin: Optional[Union["microcontroller.Pin", "machine.Pin"]] = None,
        reset_pin: Optional[Union["microcontroller.Pin", "machine.Pin"]] = None,
        debug=True,
    ) -> None:

        if debug:
            print("ExpressLink initializing...")

        self.config = Config(self)
        self.debug = debug

        self.uart = _init_uart(uart=uart, rx=rx, tx=tx, uart_id=uart_id, port=port)

        self.event_signal = _init_input_pin(event_pin)

        self.wake_signal = _init_output_pin(wake_pin)

        self.reset_signal = _init_output_pin(reset_pin)
        if reset_pin:
            _set_pin(self.reset_signal, False)
            time.sleep(1.00)
            _set_pin(self.reset_signal, True)
            time.sleep(2.00)

        if not self.self_test():
            print("ERROR: Failed ExpressLink UART self-test check!")
            self.ready = False
        else:
            self.ready = True

        if self.debug:
            print("ExpressLink ready!")

    def self_test(self):
        for _ in range(5):
            try:
                if self.debug:
                    print("ExpressLink: performing self-test...")
                self.uart.write(b"AT\r\n")
                r = _readline(self.uart, self.debug)
                if r.strip() == "OK":
                    if self.debug:
                        print("ExpressLink UART self-test successful.")
                    return True
            except Exception as e:
                if self.debug:
                    print("ExpressLink self-test error:", e)
        return False

    def passthrough(self, local_echo=True):
        if not IS_CIRCUITPYTON:
            print("Currently only supported on CircuitPython.")
            return

        print("Add this snippet to your boot.py file and power-cycle your device:")
        print("  import usb_cdc ; usb_cdc.enable(console=True, data=True)")
        print("")
        print(
            "All further input and output from the secondary USB serial interface is now passed directly to the ExpressLink UART!"
        )
        print(
            "ExpressLink passthrough activated "
            + ("with" if local_echo else "without")
            + " local echo."
        )
        print("")
        print(
            "If you are reading this message, you are connected to the primary USB serial interface!"
        )
        print(
            "Open a new serial connection to the secondary USB serial interface to send AT commands!"
        )

        while True:
            c = self.uart.in_waiting
            if c:
                r = self.uart.read(c)
                if r:
                    usb_cdc.data.write(r)

            c = usb_cdc.data.in_waiting
            if c:
                i = usb_cdc.data.read(c)
                print(i)
                if local_echo:
                    usb_cdc.data.write(i)
                self.uart.write(i)

    def cmd(self, command: str) -> Tuple[bool, str, Optional[int]]:

        assert command

        try:
            # clear any previous un-read input data
            self.uart.reset_input_buffer()
        except AttributeError:
            # only available on CircuitPython and pySerial
            pass

        command = _escape(command)
        self.uart.write(f"AT+{command}\r\n".encode())
        if self.debug:
            print("> AT+" + command)

        l = _readline(self.uart, self.debug)

        success = False
        additional_lines = 0
        error_code = None
        if l.startswith("OK"):
            success = True
            l = l[2:]  # consume the OK prefix

            r = l.find(" ")
            if r > 0:
                additional_lines = int(l[0:r])
                l = l[r:]
        elif l.startswith("ERR"):
            l = l[3:]  # consume the ERR prefix
            r = l.find(" ")
            if r > 0:
                # https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-table1
                error_code = int(l[0:r])
                l = l[r:]
            else:
                print(f"failed to parse error code: {len(l)} | {l}")
                return False, l, 2
        else:
            print(f"unexpected response: {len(l)} | {l}")
            return False, l, 2

        # read as many additional lines as needed, and concatenate them
        for _ in range(additional_lines):
            al = _readline(self.uart, debug=False, delay=False)
            if not al:
                break
            l += "\n" + al

        return success, l.strip(), error_code

    def info(self):
        print(self.config.About)
        print(self.config.Version)
        print(self.config.TechSpec)
        print(self.config.ThingName)
        print(self.config.CustomName)
        print(self.config.Endpoint)
        print(self.config.SSID)
        print(self.config.Certificate)
        self.cmd("TIME?")
        self.cmd("WHERE?")
        self.cmd("CONNECT?")

    def connect(self, non_blocking=False) -> Tuple[bool, str, Optional[int]]:
        x = "!" if non_blocking else ""
        if not non_blocking and self.debug:
            print(
                "ExpressLink connecting to the AWS Cloud... (might take a few seconds)"
            )
        return self.cmd(f"CONNECT{x}")

    def disconnect(self) -> Tuple[bool, str, Optional[int]]:
        return self.cmd("DISCONNECT")

    def sleep(self, duration, mode=None) -> Tuple[bool, str, Optional[int]]:
        if not mode:
            return self.cmd(f"SLEEP {duration}")
        else:
            return self.cmd(f"SLEEP{mode} {duration}")

    def reset(self) -> Tuple[bool, str, Optional[int]]:
        if self.reset_signal:
            _set_pin(self.reset_signal, False)
            time.sleep(1.00)
            _set_pin(self.reset_signal, True)
            time.sleep(2.00)
        # double reset is twice as good (AT commands might be stuck, so hardware reset + software reset)
        return self.cmd("RESET")

    def factory_reset(self) -> Tuple[bool, str, Optional[int]]:
        return self.cmd("FACTORY_RESET")

    def confmode(self, params=None) -> Tuple[bool, str, Optional[int]]:
        if params:
            return self.cmd(f"CONFMODE {params}")
        else:
            return self.cmd("CONFMODE AWS-IoT-ExpressLink")

    @property
    def connected(self) -> bool:
        success, line, err = self.cmd("CONNECT?")
        r = line.split(" ")
        is_connected = False
        if r[0] == "1":
            is_connected = True
        return is_connected

    @property
    def onboarded(self) -> bool:
        success, line, err = self.cmd("CONNECT?")
        r = line.split(" ")
        is_customer_account = False
        if r[1] == "1":
            is_customer_account = True
        return is_customer_account

    @property
    def time(self):
        success, line, _ = self.cmd("TIME?")
        if not success or not line.startswith("date"):
            return None

        # {date YYYY/MM/DD} {time hh:mm:ss.xx} {source}
        # date 2022/10/30 time 09:38:34.04 SNTP
        dt = namedtuple(
            "datetime",
            (
                "year",
                "month",
                "day",
                "hour",
                "minute",
                "second",
                "microsecond",
                "source",
            ),
        )
        return dt(
            year=int(line[5:9]),
            month=int(line[10:12]),
            day=int(line[13:15]),
            hour=int(line[21:23]),
            minute=int(line[24:26]),
            second=int(line[27:29]),
            microsecond=int(line[30:32]) * 10**4,
            source=line[33:],
        )

    @property
    def where(self):
        success, line, _ = self.cmd("WHERE?")
        if not success or not line.startswith("date"):
            return None
        # {date} {time} {lat} {long} {elev} {accuracy} {source}
        return line

    def get_event(self) -> Tuple[int, int, str, str]:
        if self.event_signal and not _is_pin_high(self.event_signal):
            return None, None, None, None

        success, line, _ = self.cmd("EVENT?")
        if (success and not line) or not success:
            return None, None, None, None

        if self.event_signal and hasattr(self.event_signal, "update"):
            # update signal state after getting an event and debounce signal
            self.event_signal.update()
            self.event_signal.update()
            self.event_signal.update()

        # https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-event-handling.html
        event_id, parameter, mnemonic, detail = re.match(
            r"(\d+) (\d+) (\S+)( \S+)?", line
        ).groups()
        return int(event_id), int(parameter), mnemonic, detail

    def subscribe(
        self, topic_index: int, topic_name: str
    ) -> Tuple[bool, str, Optional[int]]:
        if topic_name:
            self.config.set_topic(topic_index, topic_name)
        return self.cmd(f"SUBSCRIBE{topic_index}")

    def unsubscribe(
        self, *, topic_index: int = None, topic_name: str = None
    ) -> Tuple[bool, str, Optional[int]]:
        if topic_name and not topic_index:
            topic_index = self.config.topics.values().index(topic_name)
        return self.cmd(f"UNSUBSCRIBE{topic_index}")

    def get_message(
        self, topic_index: int = None, topic_name: str = None
    ) -> Tuple[Union[str, bool], Optional[str]]:

        if topic_name and not topic_index:
            topic_index = self.config.topics.values().index(topic_name)
        elif topic_index and topic_index > 0:
            topic_name = self.config.topics[topic_index]
        elif topic_index is None:
            topic_index = ""

        success, line, error_code = self.cmd(f"GET{topic_index}")
        if topic_index == "" or topic_index == 0:
            # next message pending or unassigned topic
            if success and line:
                topic_name, message = line.split("\n", 1)
                return topic_name, message
            else:
                return True, None
        else:
            # indicated topic with index
            if success:
                return topic_name, line
            else:
                return False, error_code

    def send(
        self, *, topic_index: int = None, topic_name: str = None, message: str = None
    ) -> Tuple[bool, str, Optional[int]]:
 
        return self.publish(
            topic_index=topic_index, topic_name=topic_name, message=message
        )

    def publish(
        self, *, topic_index: int = None, topic_name: str = None, message: str = None
    ) -> Tuple[bool, str, Optional[int]]:

        assert message
        if topic_name and not topic_index:
            topic_index = self.config.topics.values().index(topic_name)
        return self.cmd(f"SEND{topic_index} {message}")

    @property
    def ota_state(self) -> Tuple[int, str]:

        _, line, _ = self.cmd("OTA?")
        r = line.split(" ", 1)
        code = int(r[0])
        detail = None  # detail is optional
        if len(r) == 2:
            detail = r[1]
        return code, detail

    def ota_accept(self):

        return self.cmd("OTA ACCEPT")

    def ota_read(self, count: int):

        return self.cmd(f"OTA READ {count}")

    def ota_seek(self, address: Optional[int] = None):
 
        if address:
            return self.cmd(f"OTA SEEK {address}")
        else:
            return self.cmd(f"OTA SEEK")

    def ota_apply(self):
        return self.cmd("OTA APPLY")

    def ota_close(self):
        return self.cmd("OTA CLOSE")

    def ota_flush(self):
        return self.cmd("OTA FLUSH")

    def shadow_init(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} INIT")

    def shadow_doc(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} DOC")

    def shadow_get_doc(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} GET DOC")

    def shadow_update(self, new_state: str, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} UPDATE {new_state}")

    def shadow_get_update(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} GET UPDATE")

    def shadow_subscribe(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} SUBSCRIBE")

    def shadow_unsubscribe(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} UNSUBSCRIBE")

    def shadow_get_delta(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} GET DELTA")

    def shadow_delete(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} DELETE")

    def shadow_get_delete(self, index: Union[int, str] = ""):
        return self.cmd(f"SHADOW{index} GET DELETE")
