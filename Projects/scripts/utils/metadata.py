#******************************************************************************
# * @file           : metadata.py
# * @brief          : Python Utility to interact with metadata menu
# ******************************************************************************
# * @attention
# *
# * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
# * All rights reserved.</center></h2>
# *
# * This software component is licensed by ST under BSD 3-Clause license,
# * the "License"; You may not use this file except in compliance with the
# * License. You may obtain a copy of the License at:
# *                        opensource.org/licenses/BSD-3-Clause
# ******************************************************************************

import time

class metadata:
    OKAY_MESSAGE = 'STM32 OK'
    CONNECTED_MESSAGE = '[INFO] Connected to AWS'

    CMD = { 'MODIFY_ENDOINT'  : '0',
            'MODIFY_SSID'     : '1',
            'MODIFY_PASSWORD' : '2',
            'MODIFY_APN'      : '3',
            'STORAGE_WRITE'   : '4',
            'STORAGE_ERASE'   : '5',
            'SET_DEFAULT'     : '6',
            'GET_CERT'        : '7',
            'GET_THING_NAME'  : '8',
            'RESET'           : '9',
            'PASSTHROUGH'     : 'A'}

    def __init__(self, serial, debug=False):

        self.serial = serial
        self.debug = debug
            
    def wait_for_message(self, msg, delay=20) -> bool:
        start_time = time.time()
        while True:
            line = self.read_line()
            if(time.time() - start_time) > delay:
                return False
            if msg in line:
                return True
            
    def read_line(self, delay=True) -> str:
        line = self.serial.readline().decode("utf-8", errors='ignore')
        line.strip('\r\n')
        if(self.debug):
            if(line):
                print("STM32 -> " + line.strip('\r\n'))
        return line

    def read(self, delay=True) -> str:
        line = self.serial.readline().decode("utf-8", errors='ignore')
        if(self.debug):
            if(line):
                print("STM32 -> " + line.strip('\r\n'))
        return line

    def write_line(self, line, delay=True):
        self.serial.flushInput()
        if(self.debug):
            print("STM32 <- " + line.strip('\r\n'))
        cmd = bytes(line, encoding='utf-8')
        self.serial.write(cmd)   

    def enable_menu(self) -> bool:
        self.write_line("STM32")
        data = self.read_line()
        if(data.find("ACK") != 0):
            return False
        
        self.read_all()

        return True

    def read_all(self):
        data = self.read_line()
        while(data):
            data = self.read_line()

    def set_endpoint(self, endpoint):
        self.read_all()
        self.write_line(self.CMD['MODIFY_ENDOINT'])
        self.read_all()
        self.write_line(endpoint+'\r\n')
        self.read_all()

    def set_wifi_ssid(self, ssid):
        self.write_line(self.CMD['MODIFY_SSID'])
        self.read_all()
        self.write_line(ssid+'\r\n')
        self.read_all()

    def set_wifi_password(self, password):
        self.write_line(self.CMD['MODIFY_PASSWORD'])
        self.read_all()
        self.write_line(password+'\r\n')
        self.read_all()
        self.read_all()

    def set_apn(self, apn):
        self.write_line(self.CMD['MODIFY_APN'])
        self.read_all()
        self.write_line(apn+'\r\n')
        self.read_all()
        self.read_all()       

    def write_to_storage(self):
        self.write_line(self.CMD['STORAGE_WRITE'])
        time.sleep(0.1)
        self.read_all()

    def erase_storage(self):
        self.write_line(self.CMD['STORAGE_ERASE'])
        time.sleep(0.1)
        self.read_all()

    def set_defualt(self):
        self.write_line(self.CMD['SET_DEFAULT'])
        time.sleep(0.1)
        self.read_all()

    def get_cert(self) ->str:
        self.write_line(self.CMD['GET_CERT'])
        time.sleep(0.1)
        line = self.read()
        while(line.find('-----BEGIN CERTIFICATE-----') != 0):
            line = self.read()
        cert = line
        while(line !=''):
            line = self.read()
            cert += line
            if(line.find( '-----END CERTIFICATE-----') == 0):
                line=''
        
        self.read_all()
        return cert

    def get_thing_name(self) -> str:
        self.write_line(self.CMD['GET_THING_NAME'])
        response = self.read_line()
        self.read_all()
        response = response.partition("ThingName")[2]
        tokens = response.split()
        thing_name = tokens[0]
        return thing_name

    def reset(self):
        self.write_line(self.CMD['RESET'])

    def enable_passthrough(self):
        self.write_line(self.CMD['PASSTHROUGH'])
        time.sleep(0.1)
        self.read_all()