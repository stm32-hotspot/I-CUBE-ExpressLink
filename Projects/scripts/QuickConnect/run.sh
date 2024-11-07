#!/bin/bash

# AWS keys Start

# AWS Keys End


export AWS_REGION="us-east-1"
export QC_PATH=$(pwd)

clear

python3 $QC_PATH/STM32_AWS_ExpressLink_QuickConnect.py -i --accesskey=$AWS_ACCESS_KEY_ID --secretkey=$AWS_SECRET_ACCESS_KEY  --sessiontoken=$AWS_SESSION_TOKEN --region=$AWS_REGION
