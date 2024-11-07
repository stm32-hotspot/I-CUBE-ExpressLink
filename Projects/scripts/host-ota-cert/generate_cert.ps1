
$AWS_CLI_PROFILE="default"
$COUNTRY_NAME="FR"
$STATE_OR_PROVINCE_NAME="Paris"
$LOCALITY_NAME="Paris"
$ORGANIZATION_NAME="STMicroelectronics"
$COMMON_NAME="stm32h501"

# AWS keys Start

# AWS Keys End

#$env:AWS_REGION="us-east-1"

Clear-Host

python3 .\generate_cert.py --profile $AWS_CLI_PROFILE --country $COUNTRY_NAME --province $STATE_OR_PROVINCE_NAME --locality $LOCALITY_NAME --organization  $ORGANIZATION_NAME --name $COMMON_NAME
#python .\generate_cert.py --accesskey $env:AWS_ACCESS_KEY_ID --secretkey $env:AWS_SECRET_ACCESS_KEY  --sessiontoken $env:AWS_SESSION_TOKEN --region $env:AWS_REGION  --country $COUNTRY_NAME --province $STATE_OR_PROVINCE_NAME --locality $LOCALITY_NAME --organization  $ORGANIZATION_NAME --name $COMMON_NAME
