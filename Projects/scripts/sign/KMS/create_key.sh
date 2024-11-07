#!/bin/bash

export AWSPROFILE="default"

# Update with KMS key id
export KEYDESCRIPTION="ECC_NIST_P256_key"

export QC_PATH=$(pwd)


clear

echo $AWSPROFILE
echo $KEYDESCRIPTION



python3 $QC_PATH/create_key.py --aws-profile=$AWSPROFILE --key-desc=$KEYDESCRIPTION

# Command to use default values
# python $QC_PATH/create_key.py

