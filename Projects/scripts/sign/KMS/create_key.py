#**************************************************************************************
# * @file           : create_key.py
# * @brief          : Creates a sign/verify key in AWS KMS 
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
import boto3
import json
import argparse
import datetime

DEFAULT_KEY_DESCRIPTION = 'ECC_NIST_P256_key_test'
DEFAULT_AWS_PROFILE='default'
OUTPUT_FILE='KeyMetadata'


# Parse command line arguments
parser = argparse.ArgumentParser(description='Script to create a key in AWS KMS')
parser.add_argument("--key-desc",       help="Key description",         required=False,     default=DEFAULT_KEY_DESCRIPTION)
parser.add_argument("--aws-profile",    help="aws cli profile name",    required=False,     default=DEFAULT_AWS_PROFILE)
args = parser.parse_args()


# Create boto3 session
session = boto3.session.Session(profile_name=args.aws_profile)
# Create a KMS client
kms = session.client('kms')



# Create the ECDSA key
response = kms.create_key(
    Description=args.key_desc,
    KeyUsage='SIGN_VERIFY',
    CustomerMasterKeySpec='ECC_NIST_P256',
    Origin='AWS_KMS',
    MultiRegion=True
)
# Get the key metadata
key_metadata = response['KeyMetadata']



# Append time stamp to file name
now = datetime.datetime.now()
timestamp = now.strftime("%m-%d-%Y_%H-%M-%S")
file_name = f"{OUTPUT_FILE}_{timestamp}.json"



# Write Metadata to file
def default(obj):
    if isinstance(obj, (datetime.date, datetime.datetime)):
        return obj.isoformat()

with open(file_name, 'w') as f:
    json.dump(key_metadata, f, indent=4, default=default)
