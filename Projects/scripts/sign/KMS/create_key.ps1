$AWSPROFILE='default'

# Update with KMS key id
$KEYDESCRIPTION='ECC_NIST_P256_key'

Clear-Host

$AWSPROFILE
$KEYDESCRIPTION

python .\create_key.py --aws-profile=$AWSPROFILE --key-desc=$KEYDESCRIPTION

# Command to use default values
# python .\create_key.py


