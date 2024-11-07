#******************************************************************************
# * @file           : AWSHelper.py
# * @brief          : AWSHelper calass
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
import json
import webbrowser
import os, sys
from botocore.exceptions import *
import logging
from typing import Type, Dict


ASSUME_ROLE_POLICY ={
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

class AwsHelper:
    def __init__(self, loggingLevel=logging.INFO, **kwargs):
        """
        Initializes an instance of AwsHelper .

        Parameters:
            **kwargs: Additional keyword arguments.

        Keyword Args:
            option1 (bool): Specify option 1.
            option2 (str): Specify option 2.

        Returns:
            None
        """

        

        self.logger: Type[logging.Logger] = None

        self.session: Type[boto3.Session] = None

        self.iot:      Type[boto3.client] = None
        self.s3:       Type[boto3.client] = None
        self.iam:      Type[boto3.client] = None
        self.iot:      Type[boto3.client] = None
        self.signer:   Type[boto3.client] = None

        self.region  = None
        self.userId = None
        self.account = None
        self.arn = None

        self.profile = None
        self.access_key_id = None
        self.secret_key = None
        self.session_token = None
        self.thing = None

        self.deviceArn = None

        self.initLogger(level=loggingLevel)

        self.set_attributes(**kwargs)

        self.start_session()

    def set_attributes(self, **kwargs):
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)
            else:
                raise KeyError('AwsHelper class cannot handle key: ' + key)

    def initLogger(self, level=logging.INFO, format='%(levelname)s:\t%(message)s'):
        """
        - initializes the class logger
        - redirects class logger and botocore logger to stream handler

        :param level:  Logging level, defaults to 10->logging.DEBUG
        :param format: Loggig format, defaults to %(levelname)s:\t%(message)s
        """
        logging.basicConfig(format=format, level=level)
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(level)

        # Create a botocore logger
        botocore_logger = logging.getLogger('botocore')
        botocore_logger.setLevel(logging.INFO)


    def start_session(self):
        """
        - starts an aws session with either a profile name or a combination of a region, access key, secret key, and session token
        - initializes s3, iam, iot, signer, and acm clients
        - NOTE: additional clients can be anitialized as follows: AwsHelper.session.client('[client_name]')
        """
        if(self.profile != ""):
            boto3.setup_default_session(profile_name=self.profile)
        else:
            boto3.setup_default_session(region=self.region, access_key_id=self.access_key_id, secret_access_key=self.secret_key, session_token=self.session_token)

        self.session = boto3.session.Session()
        self.s3      = self.session.client('s3')
        self.iam     = self.session.client('iam')
        self.iot     = self.session.client('iot')
        self.signer  = self.session.client('signer')
        self.acm     = self.session.client('acm')
        self.region  = self.session.region_name

        sts = self.session.client('sts')
        caller_id = sts.get_caller_identity()

        if caller_id:
            self.userId = caller_id["UserId"]
            self.account = caller_id["Account"]
            self.arn = caller_id["Arn"]
    
    def create_thing_policy(self, policy: Dict):
        """
        - Creates an iot device policy
        - redirects class logger and botocore logger to stream handler

        :param policy:  policy document passed as a dictionary
        """
        if not self.iot:
            self.start_session()

        policies = self.iot.list_policies()

        policyFound = False
        for policy in policies["policies"]:
            if policy["policyName"] == "AllowAllDev":
                policyFound = True
                self.logger.debug('AllowAllDev policy Found: ' + policy["policyArn"])
        
        
        if not policyFound:
            policy = self.iot.create_policy(
                policyName="AllowAllDev",
                policyDocument=json.dumps(policy),
            )
            self.logger.debug('AllowAllDev Policy Created')

    def register_thing_csr(self, thing_name: str, csr: str, policy: Dict):
        if not self.iot:
            self.start_session()

        assert self.iot

        self.thing = {}

        cert_response = self.iot.create_certificate_from_csr(
            certificateSigningRequest=csr, setAsActive=True
        )
        # print("CreateCertificateFromCsr response: {}".format(cert_response))
        self.thing.update(cert_response)

        create_thing_resp = self.iot.create_thing(thingName=thing_name)
        # print("CreateThing response: {}".format(create_thing_resp))
        self.thing.update(create_thing_resp)

        if not (
            "certificateArn" in self.thing
            and "thingName" in self.thing
            and "certificatePem" in self.thing
        ):
            # print("Error: Certificate creation failed.")
            pass
        else:
            # print(
            #     "Attaching thing: {} to principal: {}".format(
            #         self.thing["thingName"], self.thing["certificateArn"]
            #     )
            # )
            self.iot.attach_thing_principal(
                thingName=self.thing["thingName"],
                principal=self.thing["certificateArn"],
            )

        # Check for / create Policy
        self.create_thing_policy(policy)

        # Attach the policy to the principal.
        #print('INFO:   Attaching the "AllowAllDev" policy to the device certificate.')
        self.iot.attach_policy(
            policyName="AllowAllDev", target=self.thing["certificateArn"]
        )

        self.thing["certificatePem"] = bytes(
            self.thing["certificatePem"].replace("\\n", "\n"), "ascii"
        )

        return self.thing.copy()
    
    
    def register_thing_cert(self, thing_name:str, cert:str, policy:Dict):
        """
        - Imports a certificate, creates a thing, and defines an iot policy if they do not already exist
        - Attaches the certificate thing and policy 

        :param thing_name: device name
        :param cert: device certificate
        :param policy:  policy document passed as a dictionary
        """
        if not self.iot:
            self.start_session()

        assert self.iot

        self.thing = {}

        
        try: 
            cert_response = self.iot.register_certificate_without_ca(
                certificatePem=cert, status="ACTIVE"
            )
            self.logger.debug("RegisterCertificateWithoutCA response: {}".format(cert_response))
            self.thing.update(cert_response)
        except ClientError as e:
            if e.response['Error']['Code'] == 'ResourceAlreadyExistsException':
                self.logger.debug("Certificate already exists: " + e.response['resourceId'])
                cert_response = self.iot.describe_certificate(certificateId=e.response['resourceId'])
                self.logger.debug("DescribeCertificate response: {}".format(cert_response['certificateDescription']))
                self.thing.update(cert_response['certificateDescription'])
            else:
                print("Unexpected error: %s" % e)

        create_thing_resp = self.iot.create_thing(thingName=thing_name)
        self.logger.debug("CreateThing response: {}".format(create_thing_resp))
        self.thing.update(create_thing_resp)

        if not ("certificateArn" in self.thing and "thingName" in self.thing):
            self.logger.error("Certificate creation failed.")
        else:
            self.logger.debug("Attaching thing: {} to principal: {}".format(self.thing["thingName"], self.thing["certificateArn"]))

            self.iot.attach_thing_principal(thingName=self.thing["thingName"],principal=self.thing["certificateArn"],)

        # Check for / create Policy
        self.create_thing_policy(policy)

        # Attach the policy to the principal.
        self.logger.debug('Attaching the "AllowAllDev" policy to the device certificate.')

        self.iot.attach_policy(policyName="AllowAllDev", target=self.thing["certificateArn"])

        return self.thing.copy()
    

    def openDashboard(self, deviceName:str, bucketURL:str):
        """
        - Opens a device dashboard and exports a link shortcut in the working directory

        :param deviceName: device name
        :param bucketURL: url for bucket containing dashboard code
        """
        endpoint = self.get_endpoint()
        credentials = self.session.get_credentials()
        frozen_credentials = credentials.get_frozen_credentials()
        key = frozen_credentials.access_key
        secretKey = frozen_credentials.secret_key
        secretKey = secretKey.replace("+", "%2B")
        region = self.session.region_name

        url = bucketURL + "?KEY_ID="+ key + "&SECRET_KEY=" + secretKey + "&DeviceID=" + deviceName + "&REGION=" + region + "&IOT_ENDPOINT=" + endpoint
        self.logger.debug("Dashboard URL: " + url)

        path = os.path.join('./', deviceName+".url")
        self.logger.debug('Dashboard Shortcut Path: ' + path)

        shortcut = open(path, 'w')
        shortcut.write('[InternetShortcut]\n')
        shortcut.write('URL=%s' % url)
        shortcut.close()


        webbrowser.open(url)

    def get_thing_arn(self, thing):
        """
        - Returns the arn for a thing

        :param thing: device name
        """
        try:
            thing = self.iot.describe_thing(thingName = thing)
            deviceArn  = thing['thingArn']
            self.logger.debug(deviceArn)
        except Exception as e:
            raise ValueError(e)
        return deviceArn
    
    def get_thing_group_arn(self, group):
        """
        - Returns the arn for a thing group

        :param thing: group name
        """
        try:
            group = self.iot.describe_thing_group(thingGroupName = group)
            groupArn  = group['thingGroupArn']
            self.logger.debug(groupArn)
        except Exception as e:
            raise ValueError(e)
        return groupArn

    def get_endpoint(self):
        """
        - Returns the iot endpoint
        """
        endpoint_address = ""
        if not self.iot:
            self.iot = self.start_session()

        if self.iot:
            response = self.iot.describe_endpoint(endpointType="iot:Data-ATS")

            if "endpointAddress" in response:
                endpoint_address = response["endpointAddress"]

        return endpoint_address
    
    def upload_file_to_s3(self, file:str, bucket:str, path:str):
        """
        - Uploads a local file to your s3 bucket

        :param file: file name
        :param bucket: bucket name
        :param path: path to file
        """
        file_key = "Load/" + file
        self.logger.debug('Uploading ' + file + ' to ' + bucket + ' bucket')
        
        try:
            self.s3.upload_file(Filename = path + file , Bucket = bucket, Key = file_key)
            bucket = self.s3.list_object_versions(Bucket=bucket)
            for i in range(len(bucket["Versions"])):
                if bucket["Versions"][i]['Key'] == file_key:
                    if bucket["Versions"][i]['IsLatest']:
                        self.bucketVersion = bucket["Versions"][i]['VersionId']
                        self.bucketKey = file_key
                        self.logger.debug('bucketVersion : ' + self.bucketVersion)
                        self.logger.debug('bucketKey     : ' + self.bucketKey)
                        break
        except Exception as e:
            raise ValueError(e)

    def get_role_arn(self, role:str):
        """
        - Returns the arn for a role

        :param role: role name
        """
        try:
            role = self.iam.get_role(RoleName=role)
            roleArn = role['Role']['Arn']
            self.logger.debug('Role Arn: ' + roleArn)
        except self.iam.exceptions.NoSuchEntityException:
            raise ValueError(role + " role does not exist.")
        return roleArn
    
    def get_signer_arn(self, signer:str):
        """
        - Returns the arn for a signer and None if the profile does not exist

        :param signer: signer name
        """
        profiles = self.signer.list_signing_profiles()['profiles']

        for profile in profiles:
            if profile['profileName'] == signer:
                self.logger.debug("Signing profile " + signer + "found")
                return profile['arn']

        self.logger.debug("Signing profile " + signer + "not found")
        return None

    def create_signing_profile(self, signer:str, certArn:str=None):
        """
        - Creates a signing profile if it does not exist

        :param signer: signer name
        :param certArn: arn for certificate to attach with signing profile
        """
        try:
            signerArn = self.get_signer_arn(signer)
            if (not signerArn):
                if (certArn == '' or certArn == None):
                    self.logger.error('Signing profile requires signing cert arn for creation')
                    sys.exit(1)
                else:
                    # Create profile
                    self.signer.put_signing_profile(
                        signingParameters={
                            'certname':'ota_signer_pub'
                        },
                        profileName=signer,
                        signingMaterial={
                            'certificateArn':certArn   
                        },
                        platformId='AmazonFreeRTOS-Default'
                    )

                    signerFound = False
                    while(not signerFound):
                        signerFound = self.get_signer_arn(signer)

                    self.logger.debug("Created new signing profile: %s" % signer)

        except ClientError as error:
            raise ValueError(error)
          
    def create_bucket(self, bucket:str):
        """
        - creates an s3 bucket if it does not already exist

        :param bucket: bucket name
        """
        try:
            if (self.region == 'us-east-1'):
                self.s3.create_bucket(Bucket=bucket)
            else:
                self.s3.create_bucket(Bucket=bucket, CreateBucketConfiguration={'LocationConstraint': self.region})
            self.logger.debug(bucket + ' bucket created.')
            self.s3.put_bucket_versioning(
                Bucket=bucket,
                VersioningConfiguration={
                    'MFADelete': 'Disabled',
                    'Status': 'Enabled',
                },
            )
            self.logger.debug(bucket + ' bucket versioning enabled.')
        except ClientError as error:
            if error.response['Error']['Code'] == 'BucketAlreadyOwnedByYou':
                self.logger.debug(bucket + " bucket exists.")
                self.s3.put_bucket_versioning(
                    Bucket=bucket,
                    VersioningConfiguration={
                        'MFADelete': 'Disabled',
                        'Status': 'Enabled',
                    },
            )
            elif (error.response['Error']['Code'] == 'BucketAlreadyExists'):
                self.logger.error('The requested bucket name: '+ bucket + ' is not universally unique. Please select a different name and try again')
                sys.exit(1)
            else: 
               raise ValueError(error)
            
    def create_role(self, role:str, policy:Dict=ASSUME_ROLE_POLICY):
        """
        - Creates a role with the assume role policy by default

        :param role: role name
        :param policy: role policy, defaults to ASSUME_ROLE_POLICY
        """
        try:
            self.iam.create_role(RoleName = role, AssumeRolePolicyDocument = json.dumps(policy, indent=4))
            self.logger.debug('Role created.')
        except ClientError as error:
            if error.response['Error']['Code'] == 'EntityAlreadyExists':
                self.logger.debug(role + " Role Exists.")
            else:
                raise ValueError(error)
            
    def attach_managed_role_policy(self, role:str, policyArn:str):
        """
        - Attaches an AWS managed policy to a role

        :param role: role name
        :param policyArn: arn for desired policy to be attached
        """
        try:
            self.iam.attach_role_policy(PolicyArn = policyArn, RoleName = role)
            self.logger.debug(policyArn + " policy attached to " + role)
        except  ClientError as error:
            raise ValueError(error)
  
    def attach_inline_role_policy(self, role:str, policyName:str, policyDoc:Dict):
        """
        - Creates a attaches a new policy to a role

        :param role: role name
        :param policyName: desired policy name
        :param policyDoc: desired policy document passed as a dictionary
        """
        try:
            self.iam.put_role_policy(RoleName = role, PolicyName = policyName, PolicyDocument = json.dumps(policyDoc, indent=4))
            self.logger.debug(policyName + " policy attached to " + role)
        except  ClientError as error:
            raise ValueError(error)
            
    def push_ota_update(self, updateID:str, targetArn:str, file:str, bucket:str, signer:str, roleArn:str):
        """
        - Attempts to create an OTA job

        :param updateID: job id that must be unique
        :param deviceArn: arn for target thing or thing group
        :param file: firmware file name
        :param bucket: s3 bucket that holds the firmware image
        :param signer: signing profile name
        :param roleArn: the update role arn
        """
        self.logger.debug('Creating an OTA update')

        try:
            ota_update = self.iot.create_ota_update(
                otaUpdateId = updateID,
                protocols=['MQTT'],
                targets = [targetArn],
                files = [
                    {
                    "fileName": file,
                    'fileType': 202,
                    'fileVersion': '1',
                    "fileLocation": {
                        "s3Location": {
                        "bucket": bucket,
                        "key": self.bucketKey,
                        "version": self.bucketVersion
                        }
                    },
                    "codeSigning": {
                        "startSigningJobParameter": {
                        "signingProfileName": signer,
                        "destination": {
                            "s3Destination": {
                            "bucket": bucket
                            }
                        }
                        }
                    }
                    }
                ],
                roleArn = roleArn
            )
        except Exception as e:
            raise ValueError("Error creating OTA Job: %s" % e)
        return ota_update
            
    def get_certificates(self, keyType:str='EC_prime256v1'):
        """
        - returns a dictionary of all the certificates for a given key type

        :param keyType: signing algorithm for certificates defaults to EC_prime256v1
        """
        response = self.acm.list_certificates(
                Includes={
                    'keyTypes': [
                        keyType
                    ]
                }
            )
        
        return response['CertificateSummaryList']

    def import_certificate(self, certificate, privatekey):
        """
        - imports a signing cert key pair into acm client

        :param certificate: signing certificate EXAMPLE: certificate=certificate.public_bytes(serialization.Encoding.PEM)
        :param privateKey: private key EXAMPLE: privatekey=private_key.private_bytes(encoding=serialization.Encoding.PEM,
                                                                                 format=serialization.PrivateFormat.TraditionalOpenSSL, 
                                                                                 encryption_algorithm=serialization.NoEncryption(), )
        """
        try:
            retVal = self.acm.import_certificate(Certificate=certificate, PrivateKey=privatekey)
            return retVal
        except Exception as e:
            raise ValueError(e)
            