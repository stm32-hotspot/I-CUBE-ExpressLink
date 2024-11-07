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
    """
    Access to the ExpressLink configuration dictionary, see https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-configuration-dictionary.html

    Property names are consistent with Expresslink configuration keys.
    """

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
        """getter equivalent to: `AT+CONF? About<EOL>`"""
        return self._extract_value("About")

    @property
    def Version(self) -> str:
        """getter equivalent to: `AT+CONF? Version<EOL>`"""
        return self._extract_value("Version")

    @property
    def TechSpec(self) -> str:
        """getter equivalent to: `AT+CONF? TechSpec<EOL>`"""
        return self._extract_value("TechSpec")

    @property
    def ThingName(self) -> str:
        """getter equivalent to: `AT+CONF? ThingName<EOL>`"""
        return self._extract_value("ThingName")

    @property
    def Certificate(self) -> str:
        """getter equivalent to: `AT+CONF? Certificate<EOL>`"""
        return self._extract_value("Certificate pem").lstrip("pem").strip()

    @property
    def CustomName(self) -> str:
        """
        getter equivalent to: `AT+CONF? CustomName<EOL>`

        setter equivalent to: `AT+CONF CustomName={value}<EOL>`
        """
        return self._extract_value("CustomName")

    @CustomName.setter
    def CustomName(self, value: str) -> str:
        return self._set_value("CustomName", value)

    @property
    def Endpoint(self) -> str:
        """
        getter equivalent to: `AT+CONF? Endpoint<EOL>`

        setter equivalent to: `AT+CONF Endpoint={value}<EOL>`
        """
        return self._extract_value("Endpoint")

    @Endpoint.setter
    def Endpoint(self, value: str):
        return self._set_value("Endpoint", value)

    @property
    def RootCA(self) -> str:
        """
        getter equivalent to: `AT+CONF? RootCA<EOL>`

        setter equivalent to: `AT+CONF RootCA={value}<EOL>`
        """
        return self._extract_value("RootCA pem").lstrip("pem").strip()

    @RootCA.setter
    def RootCA(self, value: str):
        return self._set_value("RootCA", value)

    @property
    def DefenderPeriod(self) -> int:
        """
        getter equivalent to: `AT+CONF? DefenderPeriod<EOL>`

        setter equivalent to: `AT+CONF DefenderPeriod={value}<EOL>`
        """
        return int(self._extract_value("DefenderPeriod"))

    @DefenderPeriod.setter
    def DefenderPeriod(self, value: int):
        return self._set_value("DefenderPeriod", str(value))

    @property
    def HOTAcertificate(self) -> str:
        """
        getter equivalent to: `AT+CONF? HOTAcertificate<EOL>`

        setter equivalent to: `AT+CONF HOTAcertificate={value}<EOL>`
        """
        return self._extract_value("HOTAcertificate pem").lstrip("pem").strip()

    @HOTAcertificate.setter
    def HOTAcertificate(self, value: str):
        return self._set_value("HOTAcertificate", value)

    @property
    def OTAcertificate(self) -> str:
        """
        getter equivalent to: `AT+CONF? OTAcertificate<EOL>`

        setter equivalent to: `AT+CONF OTAcertificate={value}<EOL>`
        """
        return self._extract_value("OTAcertificate pem").lstrip("pem").strip()

    @OTAcertificate.setter
    def OTAcertificate(self, value: str):
        return self._set_value("OTAcertificate", value)

    @property
    def SSID(self) -> str:
        """
        getter equivalent to: `AT+CONF? SSID<EOL>`

        setter equivalent to: `AT+CONF SSID={value}<EOL>`
        """
        return self._extract_value("SSID")

    @SSID.setter
    def SSID(self, value: str):
        return self._set_value("SSID", value)

    @property
    def Passphrase(self):
        raise RuntimeError("write-only persistent key")

    @Passphrase.setter
    def Passphrase(self, value: str):
        """
        getter raises `RuntimeError` due to write-only key
        setter equivalent to: `AT+CONF Passphrase={value}<EOL>`
        """
        return self._set_value("Passphrase", value)

    @property
    def APN(self) -> str:
        """
        getter equivalent to: `AT+CONF? APN<EOL>`

        setter equivalent to: `AT+CONF APN={value}<EOL>`
        """
        return self._extract_value("APN")

    @APN.setter
    def APN(self, value: str):
        return self._set_value("APN", value)

    @property
    def QoS(self) -> int:
        """
        getter equivalent to: `AT+CONF? QoS<EOL>`

        setter equivalent to: `AT+CONF QoS={value}<EOL>`
        """
        return int(self._extract_value("QoS"))

    @QoS.setter
    def QoS(self, value: int):
        return self._set_value("QoS", str(value))

    def get_topic(self, topic_index):
        """equivalent to: `AT+CONF? Topic{topic_index}<EOL>`"""
        topic_name = self._extract_value(f"Topic{topic_index}")
        self._topics[topic_index] = topic_name
        return topic_name

    def set_topic(self, topic_index, topic_name):
        """equivalent to: `AT+CONF Topic{topic_index}={topic_name}<EOL>`"""
        topic_name = topic_name.strip()
        self._topics[topic_index] = topic_name
        return self._set_value(f"Topic{topic_index}", topic_name)

    @property
    def topics(self):
        return self._topics.copy()

    @property
    def EnableShadow(self) -> bool:
        """
        getter equivalent to: `AT+CONF? EnableShadow<EOL>`

        setter equivalent to: `AT+CONF EnableShadow={value}<EOL>`
        """
        return bool(self._extract_value("EnableShadow"))

    @EnableShadow.setter
    def EnableShadow(self, value: Union[bool, int]):
        return self._set_value(
            "EnableShadow", "1" if bool(value) else "0"
        )  # accept bool as well as int as input

    def get_shadow(self, shadow_index):
        """equivalent to: `AT+CONF? Shadow{shadow_index}<EOL>`"""
        return self._extract_value(f"Shadow{shadow_index}")

    def set_shadow(self, shadow_index, shadow_name):
        """equivalent to: `AT+CONF Shadow{shadow_index}={shadow_name}<EOL>`"""
        return self._set_value(f"Shadow{shadow_index}", shadow_name)

    @property
    def ShadowToken(self) -> str:
        """
        getter equivalent to: `AT+CONF? ShadowToken<EOL>`

        setter equivalent to: `AT+CONF ShadowToken={value}<EOL>`
        """
        return self._extract_value("ShadowToken")

    @ShadowToken.setter
    def ShadowToken(self, value: str):
        return self._set_value("ShadowToken", value)


class ExpressLink:
    BAUDRATE = 115200
    """
    The default UART configuration shall be 115200, 8, N, 1 (baud rate: 115200; data bits: 8; parity: none; stop bits: 1).
    There is no hardware or software flow control for UART communications.
    See https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-commands-introduction
    """

    TIMEOUT = 0.1  # CircuitPython has a maxium of 100 seconds.
    """
    The maximum runtime for every command must be listed in the datasheet.
    No command can take more than 120 seconds to complete (the maximum time for a TCP connection timeout).
    See https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-response-timeout
    """

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
        """
        Depending on the platform:
          * ready-to-go `uart` object with ExpressLink-compatible parameters.
          * on CircuitPython/MicroPython: use RX+TX pins to auto-initialize a new UART object.
          * on Linux/macOS/Windows: use port name when using pySerial.

        :param uart: an already initialized UART object
        :param rx: the RX Pin used to create a new UART object, e.g., `Pin(1)` or `board.GP1`
        :param tx: the TX Pin used to create a new UART object, e.g., `Pin(0)` or `board.GP0`
        :param uart_id: on microcontrollers with multiple hardware UART interfaces, e.g., `0` or `1`
        :param port: using pySerial to set port name, e.g., `/dev/tty.usbserial-14440`
        :param event_pin: GPIO input pin for ExpressLink EVENT, e.g., `Pin(10)`
        :param wake_pin: GPIO output pin for ExpressLink WAKE, e.g., `Pin(11)`
        :param reset_pin: GPIO output pin for ExpressLink RESET, e.g., `Pin(12)`
        :param debug: true to enable print statements with debug log
        """

        if debug:
            print("ExpressLink initializing...")

        self.config = Config(self)
        self.debug = debug

        self.uart = _init_uart(uart=uart, rx=rx, tx=tx, uart_id=uart_id, port=port)

        # When asserted, the ExpressLink module indicates to the host processor that
        # an event has occurred (disconnect error or message received on a subscribed
        # topic) and a notification is available in the event queue waiting to be
        # delivered. It is de-asserted when the event queue is emptied. A host processor
        # can connect an interrupt input to this signal (rising edge) or can poll the
        # event queue at regular intervals.
        self.event_signal = _init_input_pin(event_pin)

        # When not asserted (high), the ExpressLink module is allowed to enter a low
        # power sleep mode. If in low power sleep mode and asserted (low), this will
        # awake the ExpressLink module.
        self.wake_signal = _init_output_pin(wake_pin)

        # When asserted (low), the ExpressLink module is held in reset (low power,
        # disconnected, all queues emptied and error conditions cleared).
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
        """
        Communication test of the UART serial interface.

        Equivalent to: `AT<EOL>` and checking for `OK<EOL>` response.

        Retried up to 5 times before ultimately failing.
        """
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
        """
        **Experimental** UART serial passthrough.

        All further input and output to the Host MCU serial port is directly passed through to the Expresslink module.

        This function does not return!

        :param local_echo: True if all input characters should be echoed back to the host immediately
        """
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
        """
        Send an AT command and read the response (can be multi-line).

        Example:
        ```
        success, line, err = el.cmd("DIAG PING 1.1.1.1")
        if not success:
            print(err)
            ...
        ```

        :param command: Expresslink AT command string. `AT+` prefix is automatically added if missing.
        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """

        assert command

        try:
            # clear any previous un-read input data
            self.uart.reset_input_buffer()
        except AttributeError:
            # only available on CircuitPython and pySerial
            pass

        # see command format definition
        # https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-commands-format

        command = _escape(command)
        self.uart.write(f"AT+{command}\r\n".encode())
        if self.debug:
            print("> AT+" + command)

        # see command response format definition
        # https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-commands.html#elpg-responses-formats
        l = _readline(self.uart, self.debug)

        success = False
        additional_lines = 0
        error_code = None
        if l.startswith("OK"):
            success = True
            l = l[2:]  # consume the OK prefix

            # optional numerical suffix [#] indicates the number of additional output lines,
            # with no additional lines expected if this suffix is omitted.
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
        # see configuration dictionary
        # https://docs.aws.amazon.com/iot-expresslink/latest/programmersguide/elpg-configuration-dictionary.html
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
        """
        Connect to the network and AWS IoT Core.

        Equivalent to: `AT+CONNECT<EOL>` or `AT+CONNECT!<EOL>`.

        :param non_blocking: `True` to use async connect with `AT+CONNECT!<EOL>`.
        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        x = "!" if non_blocking else ""
        if not non_blocking and self.debug:
            print(
                "ExpressLink connecting to the AWS Cloud... (might take a few seconds)"
            )
        return self.cmd(f"CONNECT{x}")

    def disconnect(self) -> Tuple[bool, str, Optional[int]]:
        """
        Disconnect from the network and AWS IoT Core.

        Equivalent to: `AT+DISCONNECT<EOL>`.

        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        return self.cmd("DISCONNECT")

    def sleep(self, duration, mode=None) -> Tuple[bool, str, Optional[int]]:
        """
        Sleep the ExpressLink module (power saving mode).

        Equivalent to: `AT+SLEEP {duration}<EOL>` or `AT+SLEEP{mode} {duration}<EOL>`.

        :param duration: number of seconds to sleep.
        :param mode: power saving or sleep mode (module and vendor specific).
        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        if not mode:
            return self.cmd(f"SLEEP {duration}")
        else:
            return self.cmd(f"SLEEP{mode} {duration}")

    def reset(self) -> Tuple[bool, str, Optional[int]]:
        """
        Resets the ExpressLink module (reboot). If a RESET pin is defined, it's pulled low for 1sec.

        Equivalent to `AT+RESET<EOL>`.

        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        if self.reset_signal:
            _set_pin(self.reset_signal, False)
            time.sleep(1.00)
            _set_pin(self.reset_signal, True)
            time.sleep(2.00)
        # double reset is twice as good (AT commands might be stuck, so hardware reset + software reset)
        return self.cmd("RESET")

    def factory_reset(self) -> Tuple[bool, str, Optional[int]]:
        """
        Equivalent to `AT+FACTORY_RESET<EOL>`.

        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        return self.cmd("FACTORY_RESET")

    def confmode(self, params=None) -> Tuple[bool, str, Optional[int]]:
        """
        Equivalent to `AT+CONFMODE {params}<EOL>`.
        Vendor-specific, see e.g., https://github.com/espressif/esp-aws-expresslink-eval#611-using-confmode

        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        if params:
            return self.cmd(f"CONFMODE {params}")
        else:
            return self.cmd("CONFMODE AWS-IoT-ExpressLink")

    @property
    def connected(self) -> bool:
        """
        Equivalent to `AT+CONNECT?<EOL>` and parsing the response for `CONNECTED` / `DISCONNECTED`.

        :return: true if connected, false if disconnected
        """
        success, line, err = self.cmd("CONNECT?")
        r = line.split(" ")
        is_connected = False
        if r[0] == "1":
            is_connected = True
        return is_connected

    @property
    def onboarded(self) -> bool:
        """
        Equivalent to `AT+CONNECT?<EOL>` and parsing the response for `STAGING` / `CUSTOMER`.

        :return: true if configured for customer account, false if staging account
        """
        success, line, err = self.cmd("CONNECT?")
        r = line.split(" ")
        is_customer_account = False
        if r[1] == "1":
            is_customer_account = True
        return is_customer_account

    @property
    def time(self):
        """
        Equivalent to `AT+TIME?`.

        :return: a datetime-like tuple with date and time
        """
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
        """
        Equivalent to `AT+WHERE?`.

        :return: a line with `{date} {time} {lat} {long} {elev} {accuracy} {source}`
        """
        success, line, _ = self.cmd("WHERE?")
        if not success or not line.startswith("date"):
            return None
        # {date} {time} {lat} {long} {elev} {accuracy} {source}
        return line

    def get_event(self) -> Tuple[int, int, str, str]:
        """
        Request the next event in the queue.

        Equivalent to `AT+EVENT?`. If an EVENT pin is defined, only the pin is checked.

        Example:
        ```
        event_id, parameter, mnemonic, detail = el.get_event()
        if event_id == Event.MSG:
            ...
        elif event_id == Event.OTA:
            ...
        ```

        :return: event_id as `ExpressLink.EVENT`, parameter, mnemonic, detail
        """
        # OK [{event_identifier} {parameter} {mnemonic [detail]}]{EOL}

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
        """
        Subscribe to Topic#.

        Equivalent to `AT+CONF Topic{topic_index}={topic_name}` followed by `AT+SUBSCRIBE{topic_index}`.

        :param topic_index: index to subscribe to
        :param topic_name: name of topic (None to skip setting topic in configuration dictionary)
        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        if topic_name:
            self.config.set_topic(topic_index, topic_name)
        return self.cmd(f"SUBSCRIBE{topic_index}")

    def unsubscribe(
        self, *, topic_index: int = None, topic_name: str = None
    ) -> Tuple[bool, str, Optional[int]]:
        """
        Unsubscribe from Topic#.

        Equivalent to `AT+UNSUBSCRIBE{topic_index}`.

        Can be called with a topic name or topic index.

        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        if topic_name and not topic_index:
            topic_index = self.config.topics.values().index(topic_name)
        return self.cmd(f"UNSUBSCRIBE{topic_index}")

    def get_message(
        self, topic_index: int = None, topic_name: str = None
    ) -> Tuple[Union[str, bool], Optional[str]]:
        """
        Request next message pending on the indicated topic.

        Equivalent to `AT+GET{topic_index}`.

        Can be called with a topic name or topic index.

        Example:
        ```
        topic_name, message = get_message(4)
        if topic_name and message:
            print(f"Received message on {topic_name}: {message})
        elif topic_name and not message:
            print(f"No pending messages on {topic_name}.)
        ```

        :param topic_index: topic to publish to
        :param topic_name: topic to publish to, looked up from Host-only data structure
        :return: (topic_name, message) if a message was retrieved
        :return: (True, None) if no message was pending
        :return: (False, error_code) if there was an error
        """
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
        """
        Same as `ExpressLink.publish` - use it instead.

        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        return self.publish(
            topic_index=topic_index, topic_name=topic_name, message=message
        )

    def publish(
        self, *, topic_index: int = None, topic_name: str = None, message: str = None
    ) -> Tuple[bool, str, Optional[int]]:
        """
        Publish msg on a topic selected from topic list.

        Equivalent to `AT+SEND{topic_index} {message}`.

        Can be called with a topic name or topic index.

        :param topic_index: topic to publish to
        :param topic_name: topic to publish to, looked up from Host-only data structure
        :param message: raw message to publish, typically JSON-encoded
        :return: success, response (without `OK ` prefix), error (including `ERR...` prefix)
        """
        assert message
        if topic_name and not topic_index:
            topic_index = self.config.topics.values().index(topic_name)
        return self.cmd(f"SEND{topic_index} {message}")

    @property
    def ota_state(self) -> Tuple[int, str]:
        """
        Fetches the current state of the OTA process.

        Equivalent to `AT+OTA?`.

        :return: tuple with code as `ExpressLink.OTACodes` and detail
        """
        _, line, _ = self.cmd("OTA?")
        r = line.split(" ", 1)
        code = int(r[0])
        detail = None  # detail is optional
        if len(r) == 2:
            detail = r[1]
        return code, detail

    def ota_accept(self):
        """
        Allow the OTA operation to proceed.

        Equivalent to `AT+OTA ACCEPT<EOL>`.
        """
        return self.cmd("OTA ACCEPT")

    def ota_read(self, count: int):
        """
        Requests the next # bytes from the OTA buffer.

        Equivalent to `AT+OTA READ {count}<EOL>`.
        """
        return self.cmd(f"OTA READ {count}")

    def ota_seek(self, address: Optional[int] = None):
        """
        Moves the read pointer to an absolute address.

        Equivalent to `AT+OTA SEEK<EOL>` or `AT+OTA SEEK {address}<EOL>`.
        """
        if address:
            return self.cmd(f"OTA SEEK {address}")
        else:
            return self.cmd(f"OTA SEEK")

    def ota_apply(self):
        """
        Authorize the ExpressLink module to apply the new image.

        Equivalent to `AT+OTA APPLY<EOL>`.
        """
        return self.cmd("OTA APPLY")

    def ota_close(self):
        """
        The host OTA operation is completed.

        Equivalent to `AT+OTA CLOSE<EOL>`.
        """
        return self.cmd("OTA CLOSE")

    def ota_flush(self):
        """
        The contents of the OTA buffer are emptied.

        Equivalent to `AT+OTA FLUSH<EOL>`.
        """
        return self.cmd("OTA FLUSH")

    def shadow_init(self, index: Union[int, str] = ""):
        """
        Initialize communication with the Device Shadow service.

        Equivalent to `AT+SHADOW{index} INIT<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} INIT")

    def shadow_doc(self, index: Union[int, str] = ""):
        """
        Request a Device Shadow document.

        Equivalent to `AT+SHADOW{index} DOC<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} DOC")

    def shadow_get_doc(self, index: Union[int, str] = ""):
        """
        Retrieve a device shadow document.

        Equivalent to `AT+SHADOW{index} GET DOC<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} GET DOC")

    def shadow_update(self, new_state: str, index: Union[int, str] = ""):
        """
        Request a device shadow document update.

        Equivalent to `AT+SHADOW{index} UPDATE {new_state}<EOL>`.

        :param new_state: JSON-encoded shadow document
        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} UPDATE {new_state}")

    def shadow_get_update(self, index: Union[int, str] = ""):
        """
        Retrieve a device shadow update response.

        Equivalent to `AT+SHADOW{index} GET UPDATE<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} GET UPDATE")

    def shadow_subscribe(self, index: Union[int, str] = ""):
        """
        Subscribe to a device shadow document.

        Equivalent to `AT+SHADOW{index} SUBSCRIBE<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} SUBSCRIBE")

    def shadow_unsubscribe(self, index: Union[int, str] = ""):
        """
        Unsubscribe from a device shadow document.

        Equivalent to `AT+SHADOW{index} UNSUBSCRIBE<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} UNSUBSCRIBE")

    def shadow_get_delta(self, index: Union[int, str] = ""):
        """
        Retrieve a Shadow Delta message.

        Equivalent to `AT+SHADOW{index} GET DELTA<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} GET DELTA")

    def shadow_delete(self, index: Union[int, str] = ""):
        """
        Request the deletion of a Shadow document.

        Equivalent to `AT+SHADOW{index} DELETE<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} DELETE")

    def shadow_get_delete(self, index: Union[int, str] = ""):
        """
        Request a Shadow delete response.

        Equivalent to `AT+SHADOW{index} GET DELETE<EOL>`.

        :param index: if empty (default), the unnamed shadow is used
        """
        return self.cmd(f"SHADOW{index} GET DELETE")
