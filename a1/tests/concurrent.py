import subprocess
from subprocess import PIPE
import time

def main():
    server = subprocess.Popen(['../server', '--client_ids=1,2,3', '--port=8090'])

    client1 = subprocess.Popen(['../client', '--client_id=1', '--server=localhost', '--port=8090'], stdin=PIPE)
    client2 = subprocess.Popen(['../client', '--client_id=2', '--server=localhost', '--port=8090', '--file=scripts/script-a-very-long.txt'], stdout=PIPE)
    client3 = subprocess.Popen(['../client', '--client_id=3', '--server=localhost', '--port=8090', '--file=scripts/script-b-very-long.txt'], stdout=PIPE)

    client2.wait()
    client3.wait()

    client1.stdin.write('<<quit>>'.encode('utf-8'))
    server.terminate()

if __name__ == '__main__':
    main()