#!/bin/bash

#******************************************************************************
#* @file    flash_binary.sh
#* @author  MCD Application Team
#* @brief  Flash the bootloader and the main application
#******************************************************************************
# * Copyright (c) 2021 STMicroelectronics.
#
# * All rights reserved.
#
# * This software is licensed under terms that can be found in the LICENSE file
#
# * in the root directory of this software component.
#
# * If no LICENSE file comes with this software, it is provided AS-IS.
#******************************************************************************


BOARD="B-U585I-IOT02A"
# BOARD="NUCLEO-G071RB"

# Check if it's a macOS system
if [ "$(uname)" = "Darwin" ]; then
    ST_PROGRAMMER_PATH="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI"
# Check if it's a Linux system
elif [ "$(uname)" = "Linux" ]; then
    ST_PROGRAMMER_PATH="$HOME/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI"
else
    echo "Unsupported operating system: $(uname)"
    exit 1
fi


echo "STM32CubeProgrammerPath: $ST_PROGRAMMER_PATH"
echo "Board: $BOARD"


$ST_PROGRAMMER_PATH -c port=swd INDEX=0 mode=UR -e ALL
$ST_PROGRAMMER_PATH -C port=SWD -W ./${BOARD}/Debug/${BOARD}_SecureBoot.bin 0x08000000 -V
$ST_PROGRAMMER_PATH -c port=SWD -W ../FreeRTOS/${BOARD}/Debug/signed_binary.bin 0x08008000 -V -Rst
