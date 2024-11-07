### Certificates parameters (these default values can be changed)

# CA cert (only used when generating new CA)
$CA_COUNTRY='US'
$CA_ORG='MyHSM'
$CA_CN='My_CA'
$CA_START='20230101000000'
$CA_DURATION='365'

# Signer cert
$SIGNER_COUNTRY='US'
$SIGNER_ORG='Me'
$SIGNER_CN='My_ECDSA_Signer'
$SIGNER_START='20230101000000'
$SIGNER_DURATION='365'

Clear-Host

### Choose what to generate

# Uncomment to generate new CA cert, signer cert and its key pair
python .\config_TinyHSM.py --new all --CACountry=$CA_COUNTRY --CAOrg=$CA_ORG --CACn=$CA_CN --CAStart=$CA_START --CADuration=$CA_DURATION --SignerCountry=$SIGNER_COUNTRY --SignerOrg=$SIGNER_ORG --SignerCn=$SIGNER_CN --SignerStart=$SIGNER_START --SignerDuration=$SIGNER_DURATION
# Uncomment for its Debug mode
#python .\config_TinyHSM.py -d --new all --CACountry=$CA_COUNTRY --CAOrg=$CA_ORG --CACn=$CA_CN --CAStart=$CA_START --CADuration=$CA_DURATION --SignerCountry=$SIGNER_COUNTRY --SignerOrg=$SIGNER_ORG --SignerCn=$SIGNER_CN --SignerStart=$SIGNER_START --SignerDuration=$SIGNER_DURATION
 
# Uncomment to generate only new signer cert and its key pair from current CA.
#python .\config_TinyHSM.py --new signer --SignerCountry=$SIGNER_COUNTRY --SignerOrg=$SIGNER_ORG --SignerCn=$SIGNER_CN --SignerStart=$SIGNER_START --SignerDuration=$SIGNER_DURATION
# Uncomment for its Debug mode
#python .\config_TinyHSM.py -d --new signer --SignerCountry=$SIGNER_COUNTRY --SignerOrg=$SIGNER_ORG --SignerCn=$SIGNER_CN --SignerStart=$SIGNER_START --SignerDuration=$SIGNER_DURATION
