from Crypto.Cipher import AES

import base64

def get_nonce():
    return b'\xde\xad\xbe\xef'

def build_mail(ip, key):
    cipher = AES.new(key.encode('utf-8'), AES.MODE_EAX, nonce=get_nonce())
    encoded_ip = base64.b64encode(cipher.encrypt(ip.encode('utf-8')))
    return encoded_ip.decode('utf-8')

def translate_mail(mail, key):
    cipher = AES.new(key.encode('utf-8'), AES.MODE_EAX, nonce=get_nonce())
    return cipher.decrypt(base64.b64decode(mail))

def main():
    raw_mail = translate_mail('xxxx')
    print('content: ', raw_mail)

if __name__ == '__main__':
    main()