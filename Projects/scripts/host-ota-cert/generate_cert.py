#******************************************************************************
# @file    X-509.py
# @author  MCD Application Team
# @brief   Generates a self-signed X-509 certificate key pair and upload it to AWS
# ******************************************************************************
# * @attention
# *
# * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
# * All rights reserved.</center></h2>
# *
# * This software component is licensed by ST under BSD 3-Clause license,
# * the "License"; You may not use this file except in compliance with the
# * License. You may obtain a copy of the License at:
# *                        opensource.org/licenses/BSD-3-Clause
# ******************************************************************************
import sys
sys.path.append( '../' )
from utils.AWSHelper import *
import datetime
import argparse
import logging

from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.serialization import Encoding, PrivateFormat, NoEncryption

parser = argparse.ArgumentParser(description='Script to start OTA update')
parser.add_argument("-d", action="store_true", help="degub output flag")

parser.add_argument("--profile", help="AWS CLI Profile",default="default", required=False)
parser.add_argument("--region", help="Region",default="", required=False)
parser.add_argument("--accesskey", help="AWS access key",default="", required=False)
parser.add_argument("--secretkey", help="AWS secret key",default="", required=False)
parser.add_argument("--sessiontoken", help="AWS SESSION TOKEN",default="", required=False)

parser.add_argument("--country", help="Cert COUNTRY_NAME",default="US", required=False)
parser.add_argument("--province", help="Cert STATE_OR_PROVINCE_NAME",default="California", required=False)
parser.add_argument("--locality", help="Cert LOCALITY_NAME",default="San Francisco", required=False)
parser.add_argument("--organization", help="Cert ORGANIZATION_NAME",default="STMicroelectronics", required=False)
parser.add_argument("--name", help="Cert COMMON_NAME", required=True)

args=parser.parse_args()

loggerLevel = logging.INFO
if(args.d):
    loggerLevel = logging.DEBUG

logging.basicConfig(format='%(levelname)s:\t%(message)s', level=loggerLevel)
logger = logging.getLogger(__name__)

if (args.profile):
          aws = AwsHelper(profile=args.profile, loggingLevel=loggerLevel)
          logger.info("Initializing AWS Session Profile: " + args.profile)
else:
    if (args.region and args.accesskey and args.secretkey and args.sessiontoken):
        aws = AwsHelper(region=args.region,
                access_key_id=args.accesskey, 
                secret_key=args.secretkey, 
                session_token=args.sessiontoken,
                loggingLevel=loggerLevel)
        logger.info("Initializing AWS Session Access Key: " + args.accessKey)
    else:
        logger.error("Either [Profile] or [Region, Access Key, Secret Key, Session Token and Region] are required to create AWS Session.")
        exit()


def verify_certCount(keyType='EC_prime256v1') -> bool:
    certCount = len(aws.get_certificates())
    
    user = 'y'

    if (certCount >= 10):
        logger.warning("There are currently " + str(certCount) + " available " + keyType + " certificates in your AWS ACM.")
        logger.warning("Your AWS subscription may limit the number of certificates that can be imported.")
        logger.warning("For more details, please visit https://docs.aws.amazon.com/acm/latest/userguide/acm-limits.html")
        user = input("Would you like to continue importing a certificate(Y/N)? ")

    return user.lower() == 'y'

def create_self_signed_cert():
   # Testing to make sure certs files don't already exist
    private_key = ec.generate_private_key(ec.SECP256R1(), default_backend())

    # Create a self-signed certificate with permissions to issue digital signature and sign code (for our HOTA in our case)
    subject = issuer = x509.Name([
        x509.NameAttribute(NameOID.COUNTRY_NAME          , args.country),
        x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, args.province),
        x509.NameAttribute(NameOID.LOCALITY_NAME         , args.locality),
        x509.NameAttribute(NameOID.ORGANIZATION_NAME     , args.organization),
        x509.NameAttribute(NameOID.COMMON_NAME           , args.name),
    ])
    certificate = x509.CertificateBuilder().subject_name(
        subject
    ).issuer_name(
        issuer
    ).public_key(
        private_key.public_key()
    ).serial_number(
        x509.random_serial_number()
    ).not_valid_before(
        datetime.datetime.utcnow() - datetime.timedelta(days=1)
    ).not_valid_after(
        # Our certificate will be valid for 90 days
        datetime.datetime.utcnow() + datetime.timedelta(days=100)
    ).add_extension(
        #x509.SubjectAlternativeName([x509.DNSName(u"example.com")]),
        x509.KeyUsage(
            digital_signature=True,
            content_commitment=False,
            key_encipherment=False,
            data_encipherment=False,
            key_agreement=False,
            key_cert_sign=False,
            crl_sign=False,
            encipher_only=False,
            decipher_only=False
        ),
        critical=True,  
    ).add_extension(
        #x509.SubjectAlternativeName([x509.DNSName(u"example.com")]),
        x509.ExtendedKeyUsage([x509.ExtendedKeyUsageOID.CODE_SIGNING,]),
        critical=True,
    ).sign(private_key, hashes.SHA256(),backend=default_backend())

    # logger.info the certificate and private key in PEM format in the terminal
    logger.info(' HOTA Certificate : ' + str(certificate.public_bytes(serialization.Encoding.PEM)))
    logger.info(' HOTA Private key : ' + str(private_key.private_bytes(encoding=serialization.Encoding.PEM, format=serialization.PrivateFormat.TraditionalOpenSSL, encryption_algorithm=serialization.NoEncryption(), )))

    with open('ecdsasigner-priv-key.pem', 'wb+') as f:
        f.write(private_key.private_bytes( encoding=serialization.Encoding.PEM, format=serialization.PrivateFormat.TraditionalOpenSSL, encryption_algorithm=serialization.NoEncryption() ))
    
    logger.info('Your HOTA Certificate has been saved into the file ecdsasigner.pem')

    with open('ecdsasigner.pem', 'wb+') as f:
        f.write(certificate.public_bytes( serialization.Encoding.PEM, ))
    
    logger.info('Your HOTA Private key has been saved into the file ecdsasigner-priv-key.pem')

    #Upload the certificate into AWS ACM and save the returned ARN.
    response = aws.import_certificate(certificate=certificate.public_bytes(serialization.Encoding.PEM), 
                                            privatekey=private_key.private_bytes(encoding=serialization.Encoding.PEM,
                                                                                 format=serialization.PrivateFormat.TraditionalOpenSSL, 
                                                                                 encryption_algorithm=serialization.NoEncryption(), ),)
    
    logger.info('Your HOTA Certificate has been uploaded into the AWS Certificate Management service')

    arn = response['CertificateArn']
    
    logger.info('HOTA Certificate ARN : '+ arn)

    with open('CERT_ARN.txt', 'wt') as arnfile :
            arnfile.write(arn)

    logger.info('Your HOTA Certificate ARN has been saved in the file CERT_ARN.txt ')

if __name__ == "__main__":
    if (verify_certCount()):
        create_self_signed_cert()
