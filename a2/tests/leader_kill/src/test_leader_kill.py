from http.client import HTTPConnection
import json
import os
import subprocess
import time

# hostname = os.environ['HOST_HOSTNAME']
hostname = 'localhost'

def get_pair(port, key):
    pair_json = json.dumps({'key': key})
    # addr = '{}:{}'.format(port)
    headers = {'Content-type': 'application/json'}
    conn = HTTPConnection(hostname, int(port))
    conn.request('GET', '/data', pair_json, headers=headers)
    resp = conn.getresponse()
    if resp.getcode() != 200:
        print('PORT:{} GET {} request returned code: {}'.format(port, key, resp.getcode()))
        return
    raw_body = resp.read()
    body = json.loads(raw_body.decode('utf-8'))
    print('PORT:{} GOT key:{} = value:{}'.format(port, body['key'], body['value']))

def post_pair(port, key, value):
    pair_json = json.dumps({'key': key, 'value': value})
    headers = {'Content-type': 'application/json'}
    # addr = 'localhost:{}'.format(port)
    conn = HTTPConnection(hostname, int(port))
    conn.request('POST', '/data', pair_json, headers=headers)
    resp = conn.getresponse()
    if resp.getcode() != 200:
        print('PORT:{} POST request returned code: {}'.format(port, resp.getcode()))
        return
    raw_body = resp.read()
    body = json.loads(raw_body.decode('utf-8'))
    print('PORT:{} SET key:{} = value:{}'.format(port, body['key'], body['value']))

def get_leader(port):
    conn = HTTPConnection(hostname, int(port))
    conn.request('GET', '/leader')
    resp = conn.getresponse()
    if resp.getcode() != 200:
        print('GET request returned code: {}'.format(resp.getcode()))
        return None
    raw_body = resp.read()
    print('Leader port: {}'.format(raw_body.decode('utf-8')))
    return raw_body.decode('utf-8')

def main():
    s1 = subprocess.Popen(['python3',
                            '-u',
                           './server.py',
                           '--port=7777',
                           '--zookeeper=zookeeper',
                           '--zookeeper_port=2181'])
    s2 = subprocess.Popen(['python3',
                           '-u',
                           './server.py',
                           '--port=7778',
                           '--zookeeper=zookeeper',
                           '--zookeeper_port=2181'])
    s3 = subprocess.Popen(['python3',
                           '-u',
                           './server.py',
                           '--port=7779',
                           '--zookeeper=zookeeper',
                           '--zookeeper_port=2181'])

    time.sleep(3.0)

    get_leader(7777)
    get_leader(7778)
    l = get_leader(7779)
    post_pair(7777, 'a', '1')
    time.sleep(0.5)
    post_pair(7778, 'b', '2')
    time.sleep(0.5)
    post_pair(7779, 'c', '3')
    time.sleep(0.5)
    get_pair(7777, 'b')
    time.sleep(0.5)
    get_pair(7778, 'c')
    time.sleep(0.5)
    get_pair(7779, 'a')

    remain = []
    if int(l) == 7777:
        s1.kill()
        remain.append(7778)
        remain.append(7779)
    elif int(l) == 7778:
        s2.kill()
        remain.append(7777)
        remain.append(7779)
    elif int(l) == 7779:
        s3.kill()
        remain.append(7777)
        remain.append(7778)
    print('KILLING: {}'.format(l))
    # wait for zookeeper to reap epehemeral node
    print('Waiting for zookeeper to reap ephemeral node')
    time.sleep(31)
    get_leader(remain[0])
    get_leader(remain[1])
    post_pair(remain[0], 'd', '4')
    time.sleep(0.5)
    get_pair(remain[1], 'd')

    



if __name__ == '__main__':
    main()