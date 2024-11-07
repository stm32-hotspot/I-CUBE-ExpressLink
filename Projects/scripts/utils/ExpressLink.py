#******************************************************************************
# * @file           : ExpressLink.py
# * @brief          : ExpressLink calass
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

class ExpressLink:
    def __init__(self, serial, debug=False):
        self.serial = serial
        self.debug = debug

    def readline(self, delay=True) -> str:
        line = self.serial.readline().decode("utf-8", errors='ignore')
        line.strip('\r\n')
        if(self.debug):
            if(line):
                print("ExpressLink -> " + line.strip('\r\n'))
        return line
    
    def writeline(self, line, delay=True):
        if(self.debug):
            print("ExpressLink <- " + line.strip('\r\n'))
        cmd = bytes(line, encoding='utf-8')
        self.serial.write(cmd)

    def self_test(self) -> bool:
        self.writeline(line="AT\r\n")
        return self.readline().find("OK") == 0
    
    def reset(self) -> bool:
        self.writeline(line="AT+RESET\r\n")
        return self.readline().find("OK") == 0

    def factory_reset(self) -> bool:
        self.writeline("AT+FACTORY_RESET\r\n")
        return self.readline().find("OK") == 0
    
    def set_hota_cert(self, cert) -> bool:
        error = self.readline()
        while(error!=""):
          error = self.readline()
        self.writeline("AT+CONF HOTAcertificate="+cert+"\r\n")
        error = self.readline()
        return error.find("OK") == 0
    
    def get_hota_cert(self) -> str:
        self.writeline("AT+CONF? HOTAcertificate pem\r\n")
        self.readline()
        self.readline()
        data = self.readline()
        cert = data
        while(data):
            data = self.readline()
            cert += data
        return cert.strip('\r\n')
    
    def set_ota_cert(self, cert) -> bool:
        self.writeline("AT+CONF OTAcertificate="+cert+"\r\n")
        r = self.readline()
        error = self.readline()
        return error.find("OK") == 0
    
    def get_ota_cert(self) -> str:
        self.writeline("AT+CONF? OTAcertificate pem\r\n")
        self.readline()
        data = self.readline()
        cert = data
        while(data):
            data = self.readline()
            cert += data
        return cert.strip('\r\n')
    
    def get_version(self) -> str:
        return self.get_conf("Version")
    
    def get_tech_spec(self) -> str:
        return self.get_conf("TechSpec")

    def get_about(self) -> str:
        return self.get_conf("About")
    
    def get_thing_name(self) -> str:
            return self.get_conf("ThingName")
    
    def get_thing_cert(self) -> str:
        self.writeline("AT+CONF? Certificate pem\r\n")
        data = self.readline()
        while(data == ''):
            data = self.readline()
        data = self.readline()
        cert = data
        while(data):
            data = self.readline()
            cert += data
        return cert.strip('\r\n')

    def get_conf(self, conf) -> str:
        self.writeline("AT+CONF? "+conf+"\r\n")
        return self.readline().strip('\r\n').replace('OK ', '')
