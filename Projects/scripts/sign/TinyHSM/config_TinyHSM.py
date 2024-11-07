#******************************************************************************
# * @file           : config_TinyHSM.py
# * @brief          : Initialize Tiny HSM and create necessary Certificate(s) and key pairs
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
sys.path.append( '../../' )
import logging
import argparse
from utils.TinyHSM import *
from utils.stlink import *
import logging
from halo import Halo


DRIVE_NAMES = ['DIS_U585AI']
BOARDS =      ['B-U585I-IOT02A'] 

parser = argparse.ArgumentParser(description='Script to generate in STSAFE a CA certificate, a signer certificate signed by the CA and its key pair that will be use to sign the FreeRTOS binaries')
parser.add_argument("-d"                 , help="degub output flag"                                                                                                                                           , required=False , action="store_true"  )
parser.add_argument("--new"              , help="--new all : generate new CA cert, signer cert and its key pair (default) || --new signer : generate only new signer cert and its key pair from current CA"   , required=True  , default="all")
parser.add_argument("--CACountry"        , help=""   , required=False  , default="US")
parser.add_argument("--SignerCountry"    , help=""   , required=False  , default="US")
parser.add_argument("--CAOrg"            , help=""   , required=False  , default="MyHSM")
parser.add_argument("--SignerOrg"        , help=""   , required=False  , default="Me")
parser.add_argument("--CACn"             , help=""   , required=False  , default="My_CA")
parser.add_argument("--SignerCn"         , help=""   , required=False  , default="My_ECDSA_Signer")
parser.add_argument("--CAStart"          , help=""   , required=False  , default="20230101000000")
parser.add_argument("--SignerStart"      , help=""   , required=False  , default="20230101000000")
parser.add_argument("--CADuration"       , help=""   , required=False  , default="365")
parser.add_argument("--SignerDuration"   , help=""   , required=False  , default="365")



args = parser.parse_args()      

# Initialize STLink Serial communication
def init_stlink(logger):
    try:
        _stlink = stlink()
    except Exception as e:
        logger.error(e)
        logger.info("Please close any application accessing your board's COM port" )
        exit()
    if(_stlink.drive_name in DRIVE_NAMES):
        position = DRIVE_NAMES.index(_stlink.drive_name)
        logger.debug(BOARDS[position] + " initialized")
    else:
        if(_stlink.drive_name):
            logger.error("Board not supported by the script")
            logger.info("Supported boards : " + str(BOARDS))
        else:
            logger.error("Board not found")
            logger.error("If your board is connected, then please make sure you are using a USB cable with data support")
        exit()
    logger.debug("ST-Link Path      : " + _stlink.path)
    logger.debug("ST-Link port      : " + _stlink.port)
    logger.debug("ST-Link drive_name: " + _stlink.drive_name)
    return _stlink

def main(argv):

    if(args.d):
      logging.basicConfig(format='%(levelname)s:\t%(message)s', level=logging.DEBUG)
    else:
      logging.basicConfig(format='%(levelname)s:\t%(message)s', level=logging.INFO)
    logger = logging.getLogger(__name__)
    spinner = Halo(text='', spinner='dots', enabled= True)

    # Stlink and HSM init ------------------------------------
    logger.info("Initializing HSM")
    spinner.start()
    _stlink = init_stlink(logger)
    spinner.stop()
    logger.debug("Initializing STSAFE")
    hsm = TinyHSM(serial=_stlink.ser, debug=args.d)
    hsm.reset()
    hsm.stsafe_init()
    hsm.CA_init()
    hsm.signer_init()

    if args.new=='all' or args.new=='signer':

    # GEN NEW CA CERT ----------------------------------------
        if args.new=='all':
            hsm.clean()
            logger.info("Generating new CA cert in STSAFE")
            hsm.stsafe_set_new_CA_cert(country=args.CACountry, org=args.CAOrg, cn=args.CACn, start_date=args.CAStart, days=args.CADuration)

    # GEN NEW SIGNER KEY -------------------------------------
        logger.info("Generating new signer key pair in STSAFE")
        hsm.stsafe_set_new_signer_key_pair(algo="SECP256R1")
        
    # GEN NEW SIGNER CERT ------------------------------------
        logger.info("Generating new signer cert in STSAFE")
        hsm.stsafe_set_new_signer_cert(type='CA-SIGNED', country=args.SignerCountry, org=args.SignerOrg, cn=args.SignerCn, start_date=args.SignerStart, days=args.SignerDuration)

    else:
        logger.error("argument for --new is incorrect. Please choose between :   --new all   or   --new signer")
        exit()

################################
if __name__ == "__main__":
    main(sys.argv[1:])