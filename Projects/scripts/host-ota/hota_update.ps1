#$BOARD='B-L4S5I-IOT01A'
#$BOARD='B-U585I-IOT02A'
#$BOARD='NUCLEO-G0B1RE'
#$BOARD='NUCLEO-G071RB'
#$BOARD='NUCLEO-H503RB'
$BOARD='NUCLEO-H563ZI'
#$BOARD='NUCLEO-U545RE-Q'
#$BOARD='NUCLEO-U575ZI-Q'
#$BOARD='NUCLEO-WB55RG'
#$BOARD='STM32L562E-DK'

$BIN_LOCATION='..\..\FreeRTOS\'+$BOARD+'\Debug\'
$BIN_FILE=$BOARD+'_FreeRTOS.bin'
#$BIN_FILE='signed_binary.bin'

$AWS_CLI_PROFILE=''
$THING_NAME=''

$ROLE=''
$OTA_SIGNING_PROFILE=''
$S3BUCKET=''
# CERT_ARN is used only when creating new signing profile <OTA_SIGNING_PROFILE>. Ignored by the script if using existing signing profile
#$CERT_ARN=''

#Reserved for workshop
# AWS keys Start

# AWS Keys End

#$AWS_REGION="us-east-1"

Clear-Host

python .\hota_update.py --profile=$AWS_CLI_PROFILE --thing=$THING_NAME --bin-file=$BIN_FILE --bucket=$S3BUCKET --role=$ROLE --signer=$OTA_SIGNING_PROFILE --path=$BIN_LOCATION --certarn=$CERT_ARN

# Reserved for thing group updates
# python .\hota_update.py --profile=$AWS_CLI_PROFILE --thing-group=$BOARD --bin-file=$BIN_FILE --bucket=$S3BUCKET --role=$ROLE --signer=$OTA_SIGNING_PROFILE --path=$BIN_LOCATION --certarn=$CERT_ARN

#Reserved for workshop
#python .\hota_update.py                    --thing=$THING_NAME --bin-file=$BIN_FILE --bucket=$S3BUCKET --role=$ROLE --signer=$OTA_SIGNING_PROFILE --path=$BIN_LOCATION --certarn=$CERT_ARN --region =$AWS_REGION --accesskey=$AWS_ACCESS_KEY_ID --secretkey=$AWS_SECRET_ACCESS_KEY --sessiontoken=$AWS_SESSION_TOKEN
