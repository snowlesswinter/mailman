import urllib.request
import re

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

def main():
    print(get_public_ip())

if __name__ == '__main__':
    main()