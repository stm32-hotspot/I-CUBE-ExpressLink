#******************************************************************************
# * @file           : stlink.py
# * @brief          : Python Utility for ST-Link
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

import serial
import serial.tools.list_ports
import platform
import string
import os
import time
from time import monotonic
from uuid import getnode as get_mac
if "windows" in platform.system().lower():
    import win32api
import shutil

DEFAULT_BAUD = 115200
HWID = "VID:PID=0483:374"
TIMEOUT = 1.0

REVISION = 'ST-Link.py 1.0.0'

class stlink:
    def __init__(self, baud=DEFAULT_BAUD):
        self.drive_name = self.findDriveName()
        if(self.drive_name != ''):
            self.port = self.get_com()
            self.path = self.get_path()
            self.baud = baud
            self.name = self.get_name()
            self.ser = serial.Serial(self.port, self.baud, timeout=0.1, rtscts=False)

    # Find the board drive name
    def findDriveName(self):
        DriveName = ''
        op_sys = platform.system()
        if "windows" in op_sys.lower():
            for l in string.ascii_uppercase:
                if os.path.exists('%s:\\MBED.HTM' % l):
                    DriveName = win32api.GetVolumeInformation(l+":\\")[0]
        elif "linux" in op_sys.lower():
            drives = os.popen('lsblk -lp').read().split()
            for drive in drives:
                temp_path = '%s/MBED.HTM' % (drive)
                if os.path.exists(temp_path):
                    path = drive.split('/')
                    DriveName = path[len(path) - 1]
        elif ("darwin" in op_sys.lower()) or ('mac' in op_sys.lower()): # Mac
            drives = os.popen('diskutil list').read().split()
            for drive in drives:
                temp_path = '/Volumes/%s/MBED.HTM' % drive
                if os.path.exists(temp_path):
                    path = drive.split('/')
                    DriveName = path[len(path) - 1]
        return DriveName

    # Get the board com port ########################################################################################################
    def get_com(self):
        ports = serial.tools.list_ports.comports()
        for p in ports:
            if HWID in p.hwid:
                return p.device
        
        raise Exception ( ' PORT ERR ' )

    # Get the board drive ###########################################################################################################
    def get_path(self):
        USBPATH = ''
        op_sys = platform.system()
        if self.drive_name:

            if "windows" in op_sys.lower():
                for l in string.ascii_uppercase:
                    if os.path.exists('%s:\\MBED.HTM' % l):
                        if self.drive_name.lower() in win32api.GetVolumeInformation(l+":\\")[0].lower():
                            USBPATH = '%s:\\' % l
                            break
                
            elif "linux" in op_sys.lower():
                user = os.getlogin()
                temp_path = '/media/%s/%s/' % (user, self.drive_name)
                if os.path.exists(temp_path):
                    USBPATH = temp_path
                    
            elif ("darwin" in op_sys.lower()) or ('mac' in op_sys.lower()): # Mac
                temp_path = '/Volumes/%s/' % self.drive_name
                if os.path.exists(temp_path):
                    USBPATH = temp_path
                    
            else:
                raise Exception ( ' OPERATING SYSTEM ERR ' )

        if USBPATH == '':
            raise Exception ( ' BOARD NOT FOUND ERR ' )
        
        return USBPATH

    # Combines host mac address, device serial number, and com port number to return a semi unique device name ######################
    def get_name(self):
        ports = serial.tools.list_ports.comports()
        for p in ports:
            if HWID in p.hwid:
                mac = get_mac()
                device_id = 'stm32-' + hex(mac)[-5:-1] + p.serial_number[-5:] + p.device[-2:]
                return device_id
        raise Exception("Port Error")

    # Flash the board using drag and drop ###########################################################################################
    def flash_board(self, flashing_file, wait=False):
        session_os = platform.system()
        # In Windows
        if session_os == "Windows":
            cmd = 'copy "' + flashing_file + '" "' + self.path + 'File.bin"'
            err = os.system(cmd)
            if err!=0:
                raise Exception("Flashing Error")
        else:
            shutil.copy(flashing_file, self.path)
        if wait:
            self.wait()

    # Wait for characters to come acroos the serial port ############################################################################
    def wait(self):
        bytesToRead = self.ser.in_waiting
        while (self.ser.in_waiting <= bytesToRead):
            time.sleep(0.1)

