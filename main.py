from codec import build_mail

import datetime
import git
import json
import os.path
import re
import time
import urllib.request

def get_public_ip_in_iframe(url):
    print('iframe:', url)
    req = urllib.request.Request(url)
    res = urllib.request.urlopen(req)
    page = res.read()
    raw_text = str(page)
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

def publish(config, mail):
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
    index.commit(str(datetime.datetime.now()), author=author, committer=committer)

    origin = repo.remote('origin')
    origin.push()

def main_loop(config):
    while True:
        ip = get_public_ip()
        print('ip address: ', ip)
        mail = build_mail(ip, config['key'])
        print('mail: ', mail)

        publish(config, mail)

        time.sleep(60 * 15)

def main():
    config = load_config()
    main_loop(config)

if __name__ == '__main__':
    main()