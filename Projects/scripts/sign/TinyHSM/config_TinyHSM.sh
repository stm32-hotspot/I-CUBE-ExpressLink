#!/bin/bash

### Certificates parameters (these default values can be changed)

# CA cert (only used when generating new CA)
export CA_COUNTRY="US"
export CA_ORG="MyHSM"
export CA_CN="My_CA"
export CA_START="20230101000000"
export CA_DURATION="365"

# Signer cert
export SIGNER_COUNTRY="US"
export SIGNER_ORG="Me"
export SIGNER_CN="My_ECDSA_Signer"
export SIGNER_START="20230101000000"
export SIGNER_DURATION="365"

export QC_PATH=$(pwd)

clear

### Choose what to generate

# Uncomment to generate new CA cert, signer cert and its key pair
python3 $QC_PATH/config_TinyHSM.py --new all --CACountry=$CA_COUNTRY --CAOrg=$CA_ORG --CACn=$CA_CN --CAStart=$CA_START --CADuration=$CA_DURATION --SignerCountry=$SIGNER_COUNTRY --SignerOrg=$SIGNER_ORG --SignerCn=$SIGNER_CN --SignerStart=$SIGNER_START --SignerDuration=$SIGNER_DURATION
# Uncomment for its Debug mode
#python3 $QC_PATH/config_TinyHSM.py -d --new all --CACountry=$CA_COUNTRY --CAOrg=$CA_ORG --CACn=$CA_CN --CAStart=$CA_START --CADuration=$CA_DURATION --SignerCountry=$SIGNER_COUNTRY --SignerOrg=$SIGNER_ORG --SignerCn=$SIGNER_CN --SignerStart=$SIGNER_START --SignerDuration=$SIGNER_DURATION
 
# Uncomment to generate only new signer cert and its key pair from current CA.
#python3 $QC_PATH/config_TinyHSM.py --new signer --SignerCountry=$SIGNER_COUNTRY --SignerOrg=$SIGNER_ORG --SignerCn=$SIGNER_CN --SignerStart=$SIGNER_START --SignerDuration=$SIGNER_DURATION
# Uncomment for its Debug mode
#python3 $QC_PATH/config_TinyHSM.py -d --new signer --SignerCountry=$SIGNER_COUNTRY --SignerOrg=$SIGNER_ORG --SignerCn=$SIGNER_CN --SignerStart=$SIGNER_START --SignerDuration=$SIGNER_DURATION

