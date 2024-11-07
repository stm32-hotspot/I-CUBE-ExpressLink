#******************************************************************************
# * @file           : STM32_AWS_ExpressLink_QuickConnect.py
# * @brief          : STM32 AWS ExpressLink QuickConnect
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
import sys
sys.path.append( '../' )
import argparse
from utils.AWSHelper import *
from utils.stlink import *
from utils.metadata import *
import logging
from halo import Halo
import webbrowser
import os

VERSION = "1.9.0"

DRIVE_NAMES = ['DIS_L4S5VI'    , 'DIS_U585AI'    , 'NOD_G0B1RE'   , 'NODE_G071RB'  , 'NOD_H503RB'   , 'NOD_H563ZI'   , 'NOD_U575ZI'     , 'NOD_U545RE'     , 'DIS_L562QE'   , 'NODE_WB55RG'  ]
BOARDS =      ['B-L4S5I-IOT01A', 'B-U585I-IOT02A', 'NUCLEO-G0B1RE', 'NUCLEO-G071RB', 'NUCLEO-H503RB', 'NUCLEO-H563ZI', 'NUCLEO-U575ZI-Q', 'NUCLEO-U545RE-Q', 'STM32L562E-DK', 'NUCLEO-WB55RG']

BIN_FILES = {
  "DIS_L4S5VI"  : "B-L4S5I-IOT01A_FreeRTOS.bin",
  "DIS_U585AI"  : "B-U585I-IOT02A_FreeRTOS.bin",
  "NOD_G0B1RE"  : "NUCLEO-G0B1RE_FreeRTOS.bin",
  "NODE_G071RB" : "NUCLEO-G071RB_FreeRTOS.bin",  
  "NOD_H503RB"  : "NUCLEO-H503RB_FreeRTOS.bin",
  "NOD_H563ZI"  : "NUCLEO-H563ZI_FreeRTOS.bin",
  "NOD_U575ZI"  : "NUCLEO-U575ZI-Q_FreeRTOS.bin",
  "NOD_U545RE"  : "NUCLEO-U545RE-Q_FreeRTOS.bin",  
  "DIS_L562QE"  : "STM32L562E-DK_FreeRTOS.bin",
  "NODE_WB55RG" : "NUCLEO-WB55RG_FreeRTOS.bin",
}

parser = argparse.ArgumentParser(description='Version: ' + VERSION + '\nScript for quick connecting the following boards to AWS: \n' + str(BOARDS),
                                 formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument("-d",                       action="store_true",                                        help="degub output flag",       required=False  )
parser.add_argument("-i",                       action="store_true",                                        help="interactive input flag",  required=False  )
parser.add_argument("--ssid",                   default="st_iot_demo",                                      help="WiFi SSID",               required=False  )
parser.add_argument("--password",               default="stm32u585",                                        help="WiFi Password",           required=False  )
parser.add_argument("--apn",                    default="nxt20.net",                                        help="APN",                     required=False  )
parser.add_argument("--region",                 default=None,                                               help="Region",                  required=False  )
parser.add_argument("--accesskey",              default=None,                                               help="AWS access key",          required=False  )
parser.add_argument("--secretkey",              default=None,                                               help="AWS secret key",          required=False  )
parser.add_argument("--sessiontoken",           default=None,                                               help="AWS SESSION TOKEN",       required=False  )
parser.add_argument("--dashboard-profile",      default='default',                                          help="AWS CLI Profile",         required=False  )
parser.add_argument("--provision-profile",      default='default',                                          help="The thing name",          required=False  )
parser.add_argument("--dashboard-url",          default='https://cognito.d1k7n79qfjxmou.amplifyapp.com/',   help="The thing name",          required=False  )
args=parser.parse_args()

THING_POLICY = {
    "Version": "2012-10-17",
    "Statement": [{"Effect": "Allow", "Action": "iot:*", "Resource": "*"}],
}

def get_arguments(logger):
    logger.debug(args)
    if args.i:
        args.ssid = getParam(args.ssid, "Wi-Fi SSID")
        args.password = getParam(args.password, "Wi-Fi Password")
        args.apn = getParam(args.apn, "APN")

def getParam( curParam, label):
        param = input(label + " [" + curParam + "]: ").strip()

        if param:
            return param
        else:
            return curParam
        
def getBinName(drive_name):
    path = os.path.join('.', 'bin')
    path = os.path.join(path, BIN_FILES[drive_name])
    return path
    
def main(argv):
    logging.basicConfig(format='%(levelname)s:\t%(message)s', level=logging.INFO)
    logger = logging.getLogger(__name__)

    awshelperLoggerLevel = logging.INFO
    if (args.d):
        awshelperLoggerLevel = logging.DEBUG
    
    get_arguments(logger)

    logger.debug(args)

    spinner = Halo(text='', spinner='dots', enabled= not args.d)

    # Init and connet to ST-Link COM port
    logger.info("Initializing")
    spinner.start()
    try:
        _stlink = stlink()
    except Exception as e:
        logger.error(e)
        logger.info("Please close any application accessing your board's COM port" )
        exit()
    spinner.stop()

    if(_stlink.drive_name in DRIVE_NAMES):
        position = DRIVE_NAMES.index(_stlink.drive_name)
        logger.info(BOARDS[position] + " initialized")
    else:
        if(_stlink.drive_name):
            logger.error("Board not supported by STM32_AWS_ExpressLink_QuickConnect script")
            logger.info("Supported boards : " + str(BOARDS))
        else:
            logger.error("Board not found")
            logger.error("If your board is connected, then please make sure you are using a USB cable with data support")
        exit()

    logger.debug("ST-Link Path      : " + _stlink.path)
    logger.debug("ST-Link port      : " + _stlink.port)
    logger.debug("ST-Link drive_name: " + _stlink.drive_name)

    md = metadata(serial=_stlink.ser, debug=args.d)

    if(args.accesskey or args.secretkey or args.sessiontoken or args.region):
        if (args.region and args.accesskey and args.secretkey and args.sessiontoken and args.region):
            logger.info("Initializing AWS Session")
            provision = AwsHelper(access_key_id=args.accesskey, secret_access_key=args.secretkey, session_token=args.sessiontoken, region=args.region, loggingLevel=awshelperLoggerLevel)
        else:
            logger.error("Either [Profile] or [Region, Access Key, Secret Key, Session Token and Region] are required to create AWS Session.")
            exit()
    else:
        logger.info("Initializing AWS Provision Profile Session: " + args.provision_profile)
        provision = AwsHelper(profile= args.provision_profile, loggingLevel=awshelperLoggerLevel)
        logger.info("Initializing AWS Dashboard Profile Session: " +  args.dashboard_profile)
        dasboard  = AwsHelper(profile=args.dashboard_profile, loggingLevel=awshelperLoggerLevel)

    logger.info("Collecting Endpoint")
    endpoint = provision.get_endpoint()

    logger.info("Flashing " + BIN_FILES[_stlink.drive_name])
    logger.debug("STM32 Path: " + _stlink.path)
    spinner.start()
    try:
        _stlink.flash_board(getBinName(_stlink.drive_name), wait=False)
    except Exception as e:
        logger.error(e)
        if(_stlink.drive_name == 'NOD_H503RB'):
            logger.info("Please make sure JP2 amd JP6 jumpers are fitted on your board")
        else:
            logger.info("Please make sure JP3 jumper is fitted on your board")
        exit() 
    spinner.stop()

    # Wait for STM3 OK
    logger.info("Waiting for Response from STM32...")
    spinner.start()
    if(md.wait_for_message(msg=md.OKAY_MESSAGE, delay=180) == False):
        logger.error("STM32 Not found")
        exit()
    spinner.stop()
    
    # Enable the application menu
    spinner.start()
    if(md.enable_menu() == False):
        logger.error("Error enabling the metadata menu")
        exit()
    spinner.stop()

    logger.info("Collecting NAME from ExpressLink")
    spinner.start()
    ThingName=md.get_thing_name()
    spinner.stop()
    logger.info("Thing Name                     : " + ThingName)

    logger.info("Collecting CERT from ExpressLink")
    spinner.start()
    ThingCert=md.get_cert()
    with open(ThingName+".pem", "w") as f:
        f.write(ThingCert)
    spinner.stop()

    logger.info("Sending ENDPOINT to STM32      : " + endpoint)
    spinner.start()
    md.set_endpoint(endpoint)
    spinner.stop()

    logger.info("Sending Wi-Fi SSID to STM32    : " + args.ssid)
    spinner.start()
    md.set_wifi_ssid(args.ssid)
    spinner.stop()

    logger.info("Sending Wi-Fi PASSWORD to STM32: " + args.password)
    spinner.start()
    md.set_wifi_password(args.password)
    spinner.stop()

    logger.info("Sending APN to STM32           : " + args.apn)
    spinner.start()
    md.set_apn(args.apn)
    spinner.stop()

    logger.info("Saving parameters to memory")
    spinner.start()
    md.write_to_storage()
    spinner.stop()
    
    logger.info("Registering Device with AWS")
    try:
        spinner.start()
        provision.register_thing_cert(thing_name=ThingName, cert=ThingCert, policy=THING_POLICY)
        spinner.stop()
    except Exception as e:
        logger.error(e)
        md.reset()
        exit()

    logger.info("Resetting STM32")
    spinner.start()
    md.reset()
    spinner.stop()

    # Wait for Connected to AWS
    logger.info("Waiting for STM32 to connect to AWS")
    spinner.start()
    if(md.wait_for_message(msg=md.CONNECTED_MESSAGE, delay=60) == False):
        logger.error("STM32 Not found")
        exit()

    spinner.stop()

    logger.info("Connected to AWS")

    try:
        if(args.accesskey):
            dashboard_url        = 'https://'+provision.aws_Region+'.console.aws.amazon.com/iot/home?region='+provision.aws_Region+'#/test'
            webbrowser.open(dashboard_url)
        else:
            dasboard.openDashboard(deviceName=ThingName, bucketURL=args.dashboard_url)
    except Exception as e:
        logger.error(e)
        exit()

################################
if __name__ == "__main__":
    main(sys.argv[1:])