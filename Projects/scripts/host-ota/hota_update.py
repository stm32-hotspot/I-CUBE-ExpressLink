#******************************************************************************
# * @file           : hota_update.py
# * @brief          : Push Host OTA updates
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
import argparse
from utils.AWSHelper import *
import random
import time
import os

parser = argparse.ArgumentParser(description='Script to start OTA update')
parser.add_argument("-d", action="store_true", help="degub output flag")
parser.add_argument("--profile", help="AWS CLI Profile", required=False)
parser.add_argument("--bin-file", help="The binary file", required=True)
parser.add_argument("--bucket", help="The S3 bucket", required=True)
parser.add_argument("--role", help="The signing role", required=True)
parser.add_argument("--signer", help="The signer", required=True)
parser.add_argument("--path", help="Bin file path", required=True)
parser.add_argument("--region", help="Region",default="", required=False)
parser.add_argument("--accesskey", help="AWS access key",default="", required=False)
parser.add_argument("--secretkey", help="AWS secret key",default="", required=False)
parser.add_argument("--sessiontoken", help="AWS SESSION TOKEN",default="", required=False)
parser.add_argument("--certarn", help="signing certificate arn", required=False)

group = parser.add_mutually_exclusive_group(required=True)
group.add_argument("--thing-group", help="The thing group name")
group.add_argument("--thing", help="The thing name")

args=parser.parse_args()


trust_policy ={
    "Version": "2012-10-17",
    "Statement": [
    {
        "Sid": "",
        "Effect": "Allow",
        "Principal": {
            "Service": "iot.amazonaws.com"
      },
      "Action": "sts:AssumeRole"
    }
  ]
}

bucket_policy = {
	"Version": "2012-10-17",
	"Statement": [
		{
		"Effect": "Allow",
		"Action": [
			"s3:GetObjectVersion",
			"s3:GetObject",
			"s3:PutObject"
		],
		"Resource": [
			"arn:aws:s3:::"+args.bucket+"/*"
		]
		}
	]
}

AWSIoTRuleActions = "arn:aws:iam::aws:policy/service-role/AWSIoTRuleActions"
AmazonFreeRTOSOTAUpdate = "arn:aws:iam::aws:policy/service-role/AmazonFreeRTOSOTAUpdate"
AWSIoTLogging = "arn:aws:iam::aws:policy/service-role/AWSIoTLogging"
AWSIoTThingsRegistration = "arn:aws:iam::aws:policy/service-role/AWSIoTThingsRegistration"


def main(argv):
    
    loggerLevel = logging.INFO
    if (args.d):
        loggerLevel = logging.DEBUG

    logging.basicConfig(format='%(levelname)s:\t%(message)s', level=loggerLevel)
    logger = logging.getLogger(__name__)

    logger.debug(args)

    file_path = args.path+args.bin_file
    if not os.path.exists(file_path):
      logger.error(file_path + " file not found!")
      exit()

    logger.info("Initializing AWS Session")

    if (args.profile):
          aws = AwsHelper(profile=args.profile, loggingLevel=loggerLevel)
          logger.info("Initializing AWS Profile: " + args.profile)
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
            
    get_pass_policy = {
            "Version": "2012-10-17",
            "Statement": [
                {
                    "Effect": "Allow",
                    "Action": [
                        "iam:GetRole",
                        "iam:PassRole"
                    ],
                    "Resource": "arn:aws:iam::"+aws.account+":role/"+args.role
                }
            ]
        }
         
    updateID = "ExpressLink-"+str(random.randint(1, 65535))
    logger.debug("Update ID              : " + updateID)

    if(args.thing):
        targetArn = aws.get_thing_arn(args.thing)
    elif(args.thing_group):
        targetArn = aws.get_thing_group_arn(args.thing_group)
    
    logger.info("Creating Role           : " + args.role)
    aws.create_role(args.role)

    aws.attach_managed_role_policy(role=args.role, policyArn=AWSIoTRuleActions)
    aws.attach_managed_role_policy(role=args.role, policyArn=AmazonFreeRTOSOTAUpdate)
    aws.attach_managed_role_policy(role=args.role, policyArn=AWSIoTLogging)
    aws.attach_managed_role_policy(role=args.role, policyArn=AWSIoTThingsRegistration)

    aws.attach_inline_role_policy(role=args.role, policyName='get_pass_policy', policyDoc=get_pass_policy)
    aws.attach_inline_role_policy(role=args.role, policyName='bucket_policy', policyDoc=bucket_policy)

    roleArn = aws.get_role_arn(role=args.role)
    logger.debug("Role Arn               : " + roleArn)

    logger.info("Creating Signing Profile: " + args.signer)
    aws.create_signing_profile(signer=args.signer, certArn=args.certarn)
    
    logger.info("Creating S3 Bucket      : " + args.bucket)
    aws.create_bucket(bucket=args.bucket)

    logger.info("Uploading to S3 Bucket  : "+args.bin_file+" ")
    aws.upload_file_to_s3(file=args.bin_file, bucket=args.bucket, path=args.path)

    time.sleep(7)

    logger.info("Creating OTA Update     : " + updateID)
    ota = aws.push_ota_update(updateID=updateID, 
                        targetArn=targetArn, 
                        file=args.bin_file,
                        bucket=args.bucket, 
                        signer=args.signer,
                        roleArn=roleArn)
    
    logger.info('Update Arn              : ' + ota['otaUpdateArn'])
    logger.info('Update ID               : ' + ota['otaUpdateId'])
    logger.info('Update Status           : ' + ota['otaUpdateStatus'])
         
if __name__ == "__main__":
    main(sys.argv[1:])