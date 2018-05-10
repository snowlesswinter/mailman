from Crypto.Cipher import AES

import base64
import git
import json
import os.path
import re
import urllib.request

def get_public_ip_in_iframe(url):
    print('iframe:', url)
    req = urllib.request.Request(url)
    res = urllib.request.urlopen(req)
    page = res.read()
    raw_text = str(page)
    hint = '['
    pos_start = raw_text.find(hint) + len(hint)
    pos_end = raw_text[pos_start:].find(']')
    return re.findall(r'\[\d+\.\d+\.\d+\.\d+\]', raw_text)[0][1:-1]

def get_public_ip():
    url = 'http://ip138.com'
    req = urllib.request.Request(url)
    res = urllib.request.urlopen(req)
    page = res.read()
    raw_text = str(page)
    hint = 'iframe src=\"'
    pos_start = raw_text.find(hint) + len(hint)
    pos_end = raw_text[pos_start:].find('\"')

    return get_public_ip_in_iframe(raw_text[pos_start : pos_start + pos_end])

def load_config():
    with open('config.txt', 'r') as config_file:
        return json.loads(config_file.read(-1))

    raise

def build_mail(ip, key):
    nonce = b'\xde\xad\xbe\xef'
    cipher = AES.new(key.encode('utf-8'), AES.MODE_EAX, nonce=nonce)
    encoded_ip =  base64.b64encode(cipher.encrypt(ip.encode('utf-8')))
    return encoded_ip.decode('utf-8')

    #cipher1 = AES.new(key.encode('utf-8'), AES.MODE_EAX, nonce=nonce)
    #vv = cipher1.decrypt(base64.b64decode(encoded))

def main():
    config = load_config()

    ip = get_public_ip()
    print('ip address: ', ip)
    mail = build_mail(ip, config['key'])
    print('mail: ', mail)

    repo = git.Repo(config['mailbox_path'])

    mail_file_path = os.path.join(repo.working_dir, config['mail_file_name'])
    with open(mail_file_path, 'w') as mail_file:
        mail_file.write(mail)

    index = repo.index
    index.add([mail_file_path])
    diff = index.diff('HEAD')
    if len(diff) == 0:
        return

    print('new ip encountered')
    author = git.Actor('X', 'x@unknown.com')
    committer = git.Actor('X', 'x@unknown.com')
    index.commit('m', author=author, committer=committer)

if __name__ == '__main__':
    main()