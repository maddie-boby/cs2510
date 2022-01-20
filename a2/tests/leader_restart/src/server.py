#!/usr/bin/env python3
import argparse
from http.client import HTTPConnection
import logging
import os
from os import stat, sync
import sys
import threading
import time

from flask import Flask
from flask import request
from flask.json import loads
from flask.json import dumps
from flask.json import jsonify

import kazoo
from kazoo.client import KazooClient
from kazoo.client import KazooState
from kazoo.protocol.states import EventType
# from kazoo.recipe.watchers import ChildrenWatch

app = Flask(__name__)

parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True, help='The port to listen on')
parser.add_argument('--zookeeper', required=True, help='Address/IP of zookeeper service')
parser.add_argument('--zookeeper_port', required=True, help='Zookeeper port')
ns_args = parser.parse_args()
args = vars(ns_args)
logging.basicConfig()

zk_addr = '{}:{}'.format(args['zookeeper'], args['zookeeper_port'])
zk = KazooClient(hosts=[zk_addr])
zk.start()

# def zk_connection_listener(state):
#     # don't handle if we disconnect right now
#     if state == KazooState.LOST:
#         pass
#     elif state == KazooState.SUSPENDED:
#         pass
#     else:
#         pass

# zk.add_listener(zk_connection_listener)


zk_state = None
zk_state_lock = threading.Lock()

zk_path = None

port = args['port']

def zk_init_election():
    global port
    global zk
    global zk_path
    global num_children
    # wait for network stacks to come up
    time.sleep(1)
    port_str = str(port)
    port_bytes = port_str.encode('utf-8')
    zk.ensure_path('/election/')
    zk_path = zk.create('/election/node_', makepath=True, value=port_bytes, ephemeral=True, sequence=True)
    children = zk.get_children('/election')
    num_children = len(children)
    check_election(children)

init_election_thread = threading.Thread(target=zk_init_election, name='zk_init_election')
init_election_thread.setDaemon(True)
init_election_thread.start()

# hostname = os.environ['HOST_HOSTNAME']
hostname = 'localhost'

datastore = {}
datastore_lock = threading.Lock()

leader = False
leader_lock = threading.Lock()

leader_port = None
leader_port_lock = threading.Lock()

num_children = 0

peer_ports = []


def check_election(children):
    global zk
    global zk_path
    global leader
    global leader_lock
    global leader_port
    global leader_port_lock
    global peer_ports
    global num_children
    
    if len(children) > num_children:
        num_children = len(children)
        return
    num_children = len(children)
    
    old_leader_port = ''
    with leader_port_lock:
        old_leader_port = leader_port
    # children = zk.get_children('/election', watch=zk_election_watch)
    
    min_path = None
    peer_ports = []
    for ch in children:
        c = '/election/{}'.format(ch)
        p_bytes, stat = zk.get(c)
        p = p_bytes.decode('utf-8')
        peer_ports.append(p)
        if min_path == None:
            min_path = c
        elif c < min_path:
            min_path = c
    # if min_path == '/election/{}'.format(zk_path):
    if min_path == zk_path:
        with leader_lock:
            leader = True
        with leader_port_lock:
            leader_port = port
    else:
        ldr_port_bytes, stat = zk.get(min_path)
        ldr_port = ldr_port_bytes.decode('utf-8')
        with leader_port_lock:
            leader_port = ldr_port
        if old_leader_port != leader_port:
            sync_with_leader()
    old_leader_port = leader_port

@zk.ChildrenWatch('/election')
def election_watch(children):
    check_election(children)

# def zk_election_watch(event):
#     print('zk_election_watch')
#     if event.type == EventType.DELETED:
#         check_election()



def sync_with_leader():
    global leader_port_lock
    global leader_port
    global datastore_lock
    global datastore
    ldr_port = ''
    with leader_port_lock:
        ldr_port = leader_port
    addr = '{}:{}'.format(hostname, int(ldr_port))
    # conn = HTTPConnection(addr)
    conn = HTTPConnection(hostname, ldr_port)
    try:
        conn.request('GET', '/data/leader/sync')
    except:
        return
    resp = conn.getresponse()
    code = resp.getcode()
    if code != 200:
        return
    body = resp.read()
    sync_ds = loads(body)
    if sync_ds == None:
        return
    with datastore_lock:
        datastore = sync_ds

def broadcast_post(key, val, skip_port):
    global peer_ports
    for p in peer_ports:
        if p == skip_port:
            continue
        addr = '{}:{}'.format(hostname, p)
        # conn = HTTPConnection('localhost', p)
        conn = HTTPConnection(hostname, p)
        headers = {'Content-type': 'application/json'}
        try:
            conn.request('POST', '/data/follower', dumps({'key': key, 'value': val}), headers=headers)
        except:
            continue
        resp = conn.getresponse()
        # body = resp.read()
        code = resp.getcode()
        if code != 200:
            continue

def request_post(key, val):
    global leader_port
    global leader_port_lock
    global port
    ldr_port = ''
    with leader_port_lock:
        ldr_port = leader_port
    leader_ip = '{}:{}'.format(hostname, ldr_port)
    # conn = HTTPConnection('localhost', ldr_port)
    # conn = HTTPConnection('127.0.0.1', ldr_port)
    conn = HTTPConnection(hostname, int(ldr_port))
    headers = {'Content-type': 'application/json'}
    try:
        conn.request('POST', '/data/leader', dumps({'key': key, 'value': val, 'port': port}), headers=headers)
    except:
        with datastore_lock:
            datastore[key] = val
        
    resp = conn.getresponse()
    body = resp.read()
    updated_val = loads(body)
    with datastore_lock:
        datastore[key] = updated_val['value']

# called by followers to 'catch up' to the leader's view of the db
@app.route('/data/leader/sync', methods=['GET'])
def leader_sync():
    global leader_lock
    global leader
    with leader_lock:
        if not leader:
            return '', 409
    return jsonify(datastore)

# called to request a data set from the leader
@app.route('/data/leader', methods=['POST'])
def leader_set():
    global datastore
    global datastore_lock
    data = request.get_json()
    if 'key' not in data or 'value' not in data or 'port' not in data:
        return 'Error, missing info', 400
    key = data['key']
    val = data['value']
    p = data['port']
    with datastore_lock:
        datastore[key] = val
    t = threading.Thread(target=broadcast_post, args=(key, val, p), name='broadcast_post')
    t.setDaemon(True)
    t.start()
    return jsonify({'key': key, 'value': val})

# called by leader to push updates to followers
@app.route('/data/follower', methods=['POST'])
def follower_set():
    global datastore
    global datastore_lock
    data = request.get_json()
    if 'key' not in data or 'value' not in data:
        return 'Missing data', 400
    key = data['key']
    val = data['value']
    with datastore_lock:
        datastore[key] = val
    return 'Success'

# called by client to get data
# required argument in json: {"key": <key>}
# returns {"key": <key>, "value": <value>}
@app.route('/data', methods=['GET'])
def get_data():
    global datastore
    global datastore_lock
    data = request.get_json()
    key = data['key']
    if key == None:
        return 'Error, missing key', 400
    ret = None
    with datastore_lock:
        if key not in datastore:
            ret = None
        else:
            ret = datastore[key]
    if ret == None:
        return 'Key not found', 404
    return jsonify({'key': key, 'value': ret})

# called by client to post data
# required argument in json: {"key": <key>, "value": <value>}
# returns {"key": <key>, "value": <value>}
@app.route('/data', methods=['POST'])
def post_data():
    global leader
    global leader_lock
    global datastore
    global datastore_lock

    data = request.get_json()
    key = data['key']
    val = data['value']
    if key == None or val == None:
        return 'Missing data', 400
    ldr = False
    with leader_lock:
        ldr = leader
    if ldr:
        with datastore_lock:
            datastore[key] = val
        t = threading.Thread(target=broadcast_post, args=(key, val, None), name='broadcast_post')
        t.setDaemon(True)
        t.start()
    else:
        t = threading.Thread(target=request_post, args=(key, val), name='request_post')
        t.setDaemon(True)
        t.start()
    return jsonify({'key': key, 'value': val})

# used to kill server for testing purposes
@app.route('/kill', methods=['POST'])
def kill_server():
    sys.exit()

# return the leader port for testing purposes so the test knows who to kill
@app.route('/leader', methods=['GET'])
def get_leader():
    global leader_port
    return str(leader_port)


app.run('0.0.0.0', port=args['port'])
