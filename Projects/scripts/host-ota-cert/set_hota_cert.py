#******************************************************************************
# * @file           : set_hota_cert.py
# * @brief          : Set the HOTA certificate
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

from utils.stlink import *
from utils.ExpressLink import *
from utils.metadata import *

import logging
import argparse

VERSION = "2.0.0"

DRIVE_NAMES = ['DIS_L4S5VI'    , 'DIS_U585AI'    , 'NOD_G0B1RE'   , 'NODE_G071RB'  , 'NOD_H503RB'   , 'NOD_H563ZI'   , 'NOD_U575ZI'     , 'NOD_U545RE'     , 'DIS_L562QE'   , 'NODE_WB55RG'  ]
BOARDS =      ['B-L4S5I-IOT01A', 'B-U585I-IOT02A', 'NUCLEO-G0B1RE', 'NUCLEO-G071RB', 'NUCLEO-H503RB', 'NUCLEO-H563ZI', 'NUCLEO-U575ZI-Q', 'NUCLEO-U545RE-Q', 'STM32L562E-DK', 'NUCLEO-WB55RG']

parser = argparse.ArgumentParser(description='Set ExpressLink HOTA certificate')
parser.add_argument("-d",         help="degub output flag"             , action="store_true"      , required=False)
parser.add_argument("--otacert" , help="OTA certficate file"           , default=None             , required=False)
parser.add_argument("--hotacert", help="HOTA certficate file"          , default="ecdsasigner.pem", required=False)
args=parser.parse_args()
    
def open_cert(name):
    try:
        with open(name, "r") as f:
            line = f.readline().strip("\r\n")
            byte = line+"\\A"

            line = f.readline().strip("\r\n")
            while(line != "-----END CERTIFICATE-----"):
                byte += line
                line = f.readline().strip("\r\n")

        byte += "\\A"
        byte += line
    except Exception as e:
        raise ValueError (e)

    return byte
    
def main(argv):
    if(args.d):
        logging.basicConfig(format='%(levelname)s:\t%(message)s', level=logging.DEBUG)
    else:
        logging.basicConfig(format='%(levelname)s:\t%(message)s', level=logging.INFO)
    
    logger = logging.getLogger(__name__)

    # Read the signing cert
    logger.info("Reading " + args.hotacert)
    try:
        hota_cert = open_cert(args.hotacert)
        logger.info(args.hotacert + " file found")
    except ValueError as e:
        logger.error(args.hotacert + " file not found" + e)
        exit()

    # if(args.otacert) :
    #     logger.info("Reading " + args.otacert)
    #     try:
    #         ota_cert  = open_cert(args.otacert )
    #         logger.info(args.hotacert + " file found")
    #     except ValueError as e:
    #         logger.error(args.otacert + " file not found" + e)
    #         exit()

    # Init and connet to ST-Link COM port
    try:
        _stlink = stlink()
    except Exception as e:
        logger.error(e)
        logger.info("Please close any application accessing your board's COM port" )
        exit()

    if(_stlink.drive_name in DRIVE_NAMES):
        position = DRIVE_NAMES.index(_stlink.drive_name)
        logger.info(BOARDS[position])
    else:
        if(_stlink.drive_name):
            logger.error("Board not supported")
            logger.info("Supported board : " + str(BOARDS))
        else:
            logger.error("Board not found")
        exit()

    logger.debug("ST-Link Path      : " + _stlink.path)
    logger.debug("ST-Link port      : " + _stlink.port)
    logger.debug("ST-Link drive_name: " + _stlink.drive_name)
    
    logger.info("This script will Factory reset your ExpressLink module")
    logger.info("Please reset your board to continue")
    
    md = metadata(serial=_stlink.ser, debug=args.d)
    
    # Wait for STM3 OK
    if(md.wait_for_message(msg=md.OKAY_MESSAGE, delay=120) == False):
        logger.error("STM32 Not found")
        exit()

    # Enable the application menu
    if(md.enable_menu() == False):
        logger.error("Error enabling the metadata menu")
        exit()

    logger.info("Enabling PassThrough mode")
    md.enable_passthrough()

    el = ExpressLink(serial = _stlink.ser, debug=args.d)

    if(el.self_test() == False):
        logger.error("Module self test fail")
        exit()

    logger.info("Module self test successful")

    logger.info("Sending Factoy reset command")
    el.factory_reset()

    while True:
        if el.self_test():
            break

    el.self_test()
    time.sleep(5)

    logger.info("Sending HOTA Cert")
    if(el.set_hota_cert(hota_cert) == False):
        logger.error("Error setting HOTA cert")
        exit()
    logger.info("Success")

    # if(args.otacert) :
    #     logger.info("Sending OTA Cert")
    #     if(el.set_ota_cert(ota_cert)==False):
    #         logger.error("Error setting OTA cert")
    #         exit()

    logger.info("Reading HOTA Cert")
    print(el.get_hota_cert())

    # logger.info("Reading OTA Cert")
    # print(el.get_ota_cert())

    logger.info("HOTA cert configuration completed")
    logger.info("************************************************")
    logger.info("Please reset your board to exit PassThrough mode")
    logger.info("************************************************")

################################
if __name__ == "__main__":
    main(sys.argv[1:])