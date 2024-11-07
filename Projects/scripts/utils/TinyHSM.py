#******************************************************************************
# * @file           : TinyHSM.py
# * @brief          : Tiny HSM class
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

import OpenSSL
import time
import ecdsa
from ecdsa import VerifyingKey
from ecdsa.util import sigdecode_der, sigencode_string

class TinyHSM:

    def __init__(self, serial, debug=False):
        self.serial = serial
        self.debug = debug

    def readline(self, delay=True) -> str:
        lines = self.serial.readlines()
        result=''
        counter=1
        for line in lines:
            line = line.decode("utf-8", errors='ignore')
            if(self.debug):
                print("TinyHSM -> "+line.lstrip('\r\n').rstrip('\r\n'))
                time.sleep(0.5)
            if counter == 1:
                line = line.strip('OK \r\n')
                counter = 0
            line = line.strip(' OK ')
            result += line
        return result
    
    def writeline(self, line, delay=True):
        if(self.debug):
            print("TinyHSM <- " + line.strip('\r\n'))
        cmd = bytes(line, encoding='utf-8')
        self.serial.write(cmd)

    def get_conf(self, conf) -> str:
        self.writeline("AT+"+conf+"\r\n")
        return self.readline()

    def self_test(self) -> bool:
        self.writeline(line="AT\r\n")
        return self.readline().find("OK") == 0
    
    def reset(self) -> bool:
        self.writeline(line="AT+RESET\r\n")
        return self.readline().find("OK") == 0
    
    def clean(self) -> bool:
        self.writeline(line="AT+STSAFE_DECOMMISSION\r\n")
        return self.readline().find("OK") == 0
    
    def get_version(self) -> str:
        return self.get_conf("VERSION?")

    def get_about(self) -> str:
        return self.get_conf("ABOUT?")
    
    def stsafe_init(self) -> str:
        return self.get_conf("STSAFE_INIT")
    
    def signer_init(self) -> str:
        return self.get_conf("SIGNER_INIT")
    
    def CA_init(self) -> str:
        return self.get_conf("HSM_CA_INIT")
    
    def stsafe_get_ready(self) -> str:
        return self.get_conf("STSAFE_READY?")
    
    def stsafe_get_host_key(self) -> str:
        return self.get_conf("STSAFE_HOST_KEY?")
    
    def stsafe_get_serial(self) -> str:
        return self.get_conf("STSAFE_SERIAL?")

    def stsafe_get_signer_public_key(self):
        #Read x509 cert in STSAFE
        signer_cert_read = self.stsafe_get_signer_cert()
        if('ERROR' in signer_cert_read):
            raise Exception("Signer cert not present")
        signer_cert_read=signer_cert_read.lstrip('OK\r\n')
        signer_cert_read=signer_cert_read.rstrip('\r\n')
        #Save the signer in file stsafe_signer_cert.pem in PEM format
        stsafe_cert = open("stsafe_signer_cert.pem", "w")
        stsafe_cert.write(signer_cert_read)
        stsafe_cert.close()
        # Load the certificate
        cert_file = open('stsafe_signer_cert.pem', 'rb')
        cert_data = cert_file.read()
        cert_file.close()
        cert = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM, cert_data)
        # Extract the public key from the signer cert
        public_key = cert.get_pubkey()
        public_key_data = OpenSSL.crypto.dump_publickey(OpenSSL.crypto.FILETYPE_PEM, public_key)
        # Load the public key into an ecdsa VerifyingKey object
        verifying_key = VerifyingKey.from_pem(public_key_data)
        return verifying_key   
    
    def stsafe_get_signer_cert(self) -> str:
        result =  self.get_conf("SIGNER_CERT?")
        return result
        
    def stsafe_get_CA_cert(self, cert_format) -> str:
        result =  self.get_conf("HSM_CA_CERT? FORMAT="+cert_format)
        return result
    
    def stsafe_sign(self, hash_str) -> bytes:
        sign = self.get_conf("SIGNER_SIGN HASH="+hash_str)
        if(sign == ''):
            sign = self.readline()
        sign=sign[3:]
        bytes_signature = bytes.fromhex(sign)
        # Parse the DER-encoded signature
        r, s = sigdecode_der(bytes_signature, ecdsa.NIST256p.order)
        # Concatenate the r and s values into a 64-byte signature
        signature = sigencode_string(r, s, ecdsa.NIST256p.order)
        return signature

    def stsafe_get_signer_cert_info(self) -> str:
        result =  self.get_conf("SIGNER_CERT_INFO?")
        return result
        
    def stsafe_get_CA_cert_info(self) -> str:
        result =  self.get_conf("HSM_CA_CERT_INFO?")
        return result
        
    def stsafe_set_new_signer_cert(self, type, country, org, cn, start_date, days) -> str:
        return self.get_conf("SIGNER_NEW_CERT TYPE="+type+" C="+country+" O="+org+ " CN="+cn+" START="+start_date+" DAYS="+days)
    
    def stsafe_set_new_CA_cert(self, country, org, cn, start_date, days) -> str:
        return self.get_conf("HSM_CA_NEW_CERT C="+country+" O="+org+ " CN="+cn+" START="+start_date+" DAYS="+days)
    
    def stsafe_set_new_signer_key_pair(self, algo) -> str:
        return self.get_conf("SIGNER_NEW_KEY ALGO="+algo)