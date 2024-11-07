#!/bin/bash

export QC_PATH=$(pwd)

clear

python $QC_PATH/set_hota_cert.py --hotacert ecdsasigner.pem