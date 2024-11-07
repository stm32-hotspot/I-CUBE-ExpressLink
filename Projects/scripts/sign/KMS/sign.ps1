$BOARD='B-U585I-IOT02A'
# $BOARD='NUCLEO-G071RB'

# Update with KMS key id
$KEYID='mrk-XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX'

Clear-Host

$BOARD
$KEYID

python .\sign.py --board-name=$BOARD --key-id=$KEYID

