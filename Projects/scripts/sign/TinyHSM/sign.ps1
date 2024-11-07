
$BOARD='B-U585I-IOT02A'
#$BOARD='NUCLEO-G071RB'

Clear-Host

$BOARD

python .\sign.py --board-name $BOARD

# Uncomment for Debug mode
# python .\sign.py -d --board-name $BOARD
