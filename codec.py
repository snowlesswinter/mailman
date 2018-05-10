from Crypto.Cipher import AES

import base64
import json
import pyperclip

def get_nonce():
    return b'\xde\xad\xbe\xef'

def build_mail(ip, key):
    cipher = AES.new(key.encode('utf-8'), AES.MODE_EAX, nonce=get_nonce())
    encoded_ip = base64.b64encode(cipher.encrypt(ip.encode('utf-8')))
    return encoded_ip.decode('utf-8')

def translate_mail(mail, key):
    cipher = AES.new(key.encode('utf-8'), AES.MODE_EAX, nonce=get_nonce())
    return cipher.decrypt(base64.b64decode(mail)).decode('utf-8')

def load_config():
    with open('config.txt', 'r') as config_file:
        return json.loads(config_file.read(-1))

    raise

def main():
    config = load_config()
    mail_content = input('Enter mail content: ')
    raw_mail = translate_mail(mail_content, config['key'])
    pyperclip.copy(raw_mail)
    print('ip copied to clipboard: ', raw_mail)
    input()

if __name__ == '__main__':
    main()