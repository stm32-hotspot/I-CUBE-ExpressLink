#!/bin/bash

export AWS_CLI_PROFILE="default"
export COUNTRY_NAME="FR"
export STATE_OR_PROVINCE_NAME="Paris"
export LOCALITY_NAME="Paris"
export ORGANIZATION_NAME="STMicroelectronics"
export COMMON_NAME="stm32h501"


# AWS keys Start

# AWS Keys End

#export AWS_REGION="us-east-1"
export QC_PATH=$(pwd)

clear

python3 $QC_PATH/generate_cert.py --profile $AWS_CLI_PROFILE  --country $COUNTRY_NAME --province $STATE_OR_PROVINCE_NAME --locality $LOCALITY_NAME --organization  $ORGANIZATION_NAME --name $COMMON_NAME
#python $QC_PATH/generate_cert.py --accesskey $AWS_ACCESS_KEY_ID --secretkey $AWS_SECRET_ACCESS_KEY  --sessiontoken $AWS_SESSION_TOKEN --region $AWS_REGION  --country $COUNTRY_NAME --province $STATE_OR_PROVINCE_NAME --locality $LOCALITY_NAME --organization  $ORGANIZATION_NAME --name $COMMON_NAME-
