import subprocess
from subprocess import PIPE

def main():
    server = subprocess.Popen(['../server', '--client_ids=1,2,3', '--port=8090'])

    with subprocess.Popen(['../client', '--client_id=1', '--server=localhost', '--port=8090', '--file=scripts/script-short.txt']) as p:
        pass
    print('~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~')
    print('client 2')
    print('~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~')

    with subprocess.Popen(['../client', '--client_id=2', '--server=localhost', '--port=8090', '--file=scripts/script-short.txt'], stdout=PIPE) as p:
        print(p.stdout.read().decode('utf-8'))

    print('~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~')
    print('client 3')
    print('~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~')

    with subprocess.Popen(['../client', '--client_id=3', '--server=localhost', '--port=8090', '--file=scripts/script-short.txt'], stdout=PIPE) as p:
        print(p.stdout.read().decode('utf-8'))

    server.terminate()

if __name__ == '__main__':
    main()