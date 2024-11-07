#$BOARD='B-U585I-IOT02A'
$BOARD='NUCLEO-G071RB'

Clear-Host

$BOARD

python .\sign.py --board-name $BOARD

