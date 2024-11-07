
# AWS keys Start

# AWS Keys End


$env:AWS_REGION="us-east-1"

Clear-Host

python .\STM32_AWS_ExpressLink_QuickConnect.py -i --accesskey=$env:AWS_ACCESS_KEY_ID --secretkey=$env:AWS_SECRET_ACCESS_KEY  --sessiontoken=$env:AWS_SESSION_TOKEN --region=$env:AWS_REGION

