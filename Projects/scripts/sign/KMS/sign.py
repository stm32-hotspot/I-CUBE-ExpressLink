#**************************************************************************************
# * @file           : sign.py
# * @brief          : Prepends a header and appends an AWS KMS signature for a binary 
# *************************************************************************************
# * @attention
# *
# * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
# * All rights reserved.</center></h2>
# *
# * This software component is licensed by ST under BSD 3-Clause license,
# * the "License"; You may not use this file except in compliance with the
# * License. You may obtain a copy of the License at:
# *                        opensource.org/licenses/BSD-3-Clause
# **************************************************************************************

import ecdsa
from ecdsa.util import sigdecode_der, sigencode_string
import sys
import re
import hashlib
import argparse
import boto3
from pathlib import Path



BOOTHEADER="SECUREBOOT10"
VERSIONFILE = "../../../FreeRTOS/{board_name}/Inc/application_version.h"
BINFILE = "../../../FreeRTOS/{board_name}/Debug/{board_name}_FreeRTOS.bin"
OUTPUTFILE = "../../../FreeRTOS/{board_name}/Debug/signed_binary.bin"
SIGGEN = "../../../SecureBoot/{board_name}/Src/SigGen.c"
DEFAULT_AWS_PROFILE='default'

parser = argparse.ArgumentParser(description='Script to append header and sign firmware image with AWS KMS')
parser.add_argument("--board-name",     help="Name of board",                                                       required=True)
parser.add_argument("--version-file",   help="Path to application version file (default: "+VERSIONFILE+")",         required=False, default=VERSIONFILE)
parser.add_argument("--bin-file",       help="Path to firmware image (default: "+BINFILE+")",                       required=False, default=BINFILE)
parser.add_argument("--output-file",    help="Path to desired output signed binary file (default: "+OUTPUTFILE+")", required=False, default=OUTPUTFILE)
parser.add_argument("--sig-gen",        help="Path to sign gen file(default: "+SIGGEN+")",                          required=False, default=SIGGEN)
parser.add_argument("--key-id",         help="AWS KMS Key ID",                                                      required=True)
parser.add_argument("--aws-profile",    help="aws cli profile name",    required=False,     default=DEFAULT_AWS_PROFILE)
args = parser.parse_args()

args.version_file = args.version_file.format(board_name=args.board_name)
args.bin_file = args.bin_file.format(board_name=args.board_name)
args.output_file = args.output_file.format(board_name=args.board_name)
args.sig_gen = args.sig_gen.format(board_name=args.board_name)

# Create boto3 session
session = boto3.session.Session(profile_name=args.aws_profile)

# Create a KMS client
KMS = session.client('kms')


# Returns the public KMS public key
def getPubKey(key_id):
    response = KMS.get_public_key(
        KeyId=key_id
    )

    vk = ecdsa.VerifyingKey.from_der(response['PublicKey'])
    return vk


# Return a 64 byte signature for the hashed digest
def sign(digest, key_id):
    response = KMS.sign(
        KeyId=key_id,
        Message=digest,
        MessageType='DIGEST',
        SigningAlgorithm='ECDSA_SHA_256'
    )

    # Convert signature from 71 to 64 bytes
    signature = convert_signature(response['Signature'])

    return signature

# Convert 71 byte signature to 64 bytes
def convert_signature(der_signature):
    # Parse the DER-encoded signature
    r, s = sigdecode_der(der_signature, ecdsa.NIST256p.order)

    # Concatenate the r and s values into a 64-byte signature
    signature = sigencode_string(r, s, ecdsa.NIST256p.order)

    return signature

def hash(message):
    hash_digest = hashlib.sha256(message).digest()
    return hash_digest

# Open the bin file
def open_file(name):
    with open(name, "rb") as f:
        header = f.read()
    f.close()    
    return header

# Add a string to the bin file
def add_string(new_string, data):
    data += new_string.encode()
    hex_data = '00'
    data += bytes.fromhex(hex_data)
    return data
  
# add 0xFF to have a specefic allignment
def add_padding(size, data): 
    while((len(data) % size) != 0):
        hex_data = 'FF'
        new_data = bytes.fromhex(hex_data)
        data += new_data
    return data

# Read the revision number and return it as a string on the for of 0.9.1
def get_version():
    constants = dict()
    with open(args.version_file) as infile:
                for line in infile:
                    try:
                        matchFound = (re.findall(r"#define\s+(\w+)\s+(.*)", line.strip()))
                        if (len(matchFound) != 0):
                            constants.update(matchFound)
                    except Exception as e:
                            pass
    app_version = (constants["APP_VERSION_MAJOR"] + "." + constants["APP_VERSION_MINOR"] + "." + constants["APP_VERSION_BUILD"])
    return app_version          

def generate_header_file(qx, qy):
    # Get the length of the string
    qx_s = hex(qx)
    qy_s = hex(qy)

    # Open a file for writing
    file = open(args.sig_gen, 'w')

    # Write data to the file
    file.write("/* File autogenerated by sign.py script */")
    file.write('\r\n')
    file.write('#include "main.h"\n\n')
    file.write('#define SigGen_Qx_len  32\n')
    file.write('#define SigGen_Qy_len  32\n')
    file.write('\n')

    file.write('const uint8_t SigGen_Qx[SigGen_Qx_len] = {')
    for i in range(2, len(qx_s), 2):
      file.write('0x')
      file.write(qx_s[i:i+2])
      if(i < len(qx_s) - 2):
        file.write(', ')
    file.write('};\n')

    file.write('const uint8_t SigGen_Qy[SigGen_Qy_len] = {')
    for i in range(2, len(qy_s), 2):
      file.write('0x')
      file.write(qy_s[i:i+2])
      if(i < len(qy_s) - 2):
        file.write(', ')               
    file.write('};\n')

    file.close()

def main(argv):

    #Check the FreeRTOS binary is present
    file_path = Path(args.bin_file)
    if not Path.exists(file_path):
      print('[ERROR]:\t', file_path, " file not found!")
      exit()

    # # init the signing key
    vk = getPubKey(args.key_id)

    # Open the binary file
    bin = open_file(args.bin_file)
    bin = add_padding(512, bin)
    bin_size = len(bin)

    # Build Header 
    # Secure boot header revision
    boot_header = BOOTHEADER + '\0'
    header = boot_header.encode()
    header = add_padding(16, header)

    # Add App size
    app_size = str(bin_size + 512) + '\0'
    header = add_string(app_size, header)
    header = add_padding(16, header)

    # Add board name to header
    boardName = args.board_name
    header = add_string(boardName, header)
    header = add_padding(16, header)
    
    # Add the firmware revision number to the firmware
    app_version = get_version()
    header = add_string(app_version, header)
    header = add_padding(512, header)

    header += bin
   
    #hash the binary
    digest = hash(header)

    # Sign the binary 
    signature = sign(digest, args.key_id)

    # Get public key qx and qy
    qx = vk.pubkey.point.x()
    qy = vk.pubkey.point.y()
    qx_hex = hex(qx)
    qy_hex = hex(qy)

    # Print the signature on the console (For debug purpose)
    print("qx         :", qx_hex)
    print("qy         :", qy_hex)
    print("digest     :", digest.hex())
    print("Signature  :", signature.hex())

    generate_header_file(qx, qy)

    # Verify the signature (for debug purpose)
    print(vk.verify_digest(signature, digest))
    
    # Add the signature to the binary file
    header += signature 

    # Write the new binary file
    with open(args.output_file, 'wb') as f:
        f.write(header)
        f.close()

################################
if __name__ == "__main__":
    main(sys.argv[1:])

