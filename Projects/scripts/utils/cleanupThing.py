
#******************************************************************************
# * @file           : cleanupThing.py
# * @brief          : Cleans up thing created by quickConnect.py
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
import boto3
import boto3.session
import argparse
import logging

parser = argparse.ArgumentParser(description='Script to delete thing from AWS IoT core')
parser.add_argument("--profile"     , help="AWS CLI Profile" , default=None, required=False)
parser.add_argument("--thing"       , help="The thing name"   ,default=None, required=True)
parser.add_argument("--region"      , help="AWS Region"       ,default=None, required=False)
parser.add_argument("--accesskey"   , help="AWS access key"   ,default=None, required=False)
parser.add_argument("--secretkey"   , help="AWS secret key"   ,default=None, required=False)
parser.add_argument("--sessiontoken", help="AWS SESSION TOKEN",default=None, required=False)
args=parser.parse_args()

def main():
    logging.basicConfig(format='%(levelname)s:\t%(message)s')
    logger = logging.getLogger(__name__)

    thing_name = args.thing

    # Initialize Boto3 resources
    try:
        # this_session = boto3.session.Session(profile_name=PROFILE_NAME)
        this_session = boto3.session.Session(
            profile_name=args.profile,
            region_name=args.region,
            aws_access_key_id=args.accesskey,
            aws_secret_access_key=args.secretkey,
            aws_session_token=args.sessiontoken,
        )

        iot = this_session.client('iot')
    except Exception as e:
       logger.error(str(e))
       exit()

    # Get resources
    try:
        cert_arn_list = iot.list_thing_principals(thingName = thing_name)['principals']
    except Exception as e:
        logger.error("Thing " + thing_name + " not found.\n\n")
        exit()
    
    
    # Building a Dictionary {certArn: [list, of, policies]}
    certDict = {}
    for cert_arn in cert_arn_list:
        logger.info("\tCertificate Arn - " + cert_arn + "\n")

        policy_list = list(map(lambda p: p['policyName'], iot.list_attached_policies(target = cert_arn)['policies']))
        for policy in policy_list:
            logger.info("\t\tPolicy Name - " + policy + "\n")

        certDict[cert_arn] = policy_list

   
    # Detaching certificates 
    for cert_arn in cert_arn_list:
        logger.info("Detaching certificate from thing " + cert_arn + "...\n")
        iot.detach_thing_principal(thingName = thing_name, principal = cert_arn)

    
    # Detaching every policy from all the attached certs
    for cert_arn in certDict:
        for policy_name in certDict[cert_arn]:
            logger.info("Detaching " + policy_name + " from " + cert_arn + "...\n")
            iot.detach_policy(policyName = policy_name, target = cert_arn)


    # Deleting the thing
    logger.info("Deleting thing...\n")
    iot.delete_thing(thingName = thing_name)

    # Deactivating Revoking and Deleting every Cert
    for cert_arn in certDict:
        cert_id = cert_arn.partition('/')[2]
        logger.info("Deactivating certificate " + cert_id + "...\n")
        iot.update_certificate(certificateId = cert_id, newStatus = 'INACTIVE')

        logger.info("Revoking certificate...\n")
        iot.update_certificate(certificateId = cert_id, newStatus = 'REVOKED')

        logger.info("Deleting certificate...\n")
        iot.delete_certificate(certificateId = cert_id)
    
    logger.info("Finished.\n")

if __name__ == "__main__":
    main()

#************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/            