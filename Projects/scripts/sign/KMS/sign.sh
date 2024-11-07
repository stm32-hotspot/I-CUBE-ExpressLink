#!/bin/bash

export BOARD="B-U585I-IOT02A"
# export BOARD="NUCLEO-G071RB"

# Update with KMS Key ID
export KEYID="mrk-XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

export QC_PATH=$(pwd)

clear

echo $BOARD
echo $KEYID

python3 $QC_PATH/sign.py --board-name=$BOARD --key-id=$KEYID
