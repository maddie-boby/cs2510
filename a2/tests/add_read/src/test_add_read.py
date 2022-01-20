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
        print('GET request returned code: {}'.format(resp.getcode()))
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
        print('POST request returned code: {}'.format(resp.getcode()))
    raw_body = resp.read()
    body = json.loads(raw_body.decode('utf-8'))
    print('PORT:{} SET key:{} = value:{}'.format(port, body['key'], body['value']))

def get_leader(port):
    conn = HTTPConnection(hostname, int(port))
    conn.request('GET', '/leader')
    resp = conn.getresponse()
    if resp.getcode() != 200:
        print('GET request returned code: {}'.format(resp.getcode()))
    raw_body = resp.read()
    print('Leader port: {}'.format(raw_body.decode('utf-8')))

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
    get_leader(7779)
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


if __name__ == '__main__':
    main()