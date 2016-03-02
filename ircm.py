#! /usr/bin/env python3
# This program is the commandline tool for interacting with ircd.
# It will allow you to control ircd from the cmd line.

import argparse
import os
import json

def main():
    conf = read_config('config.json', 'default')
    fifo_file = "/tmp/ircd.fifo"

    parser = argparse.ArgumentParser(description = "Simple python based irc-bot daemon")

    parser.add_argument('-c', '--connect', metavar = 'host',
                        type = str, default = conf['host'],
                        help = 'Host to connect to.')

    parser.add_argument('-j', '--join', metavar = 'channel',
                        type = str, default = conf['channel'],
                        help = 'Channel to connect to.')

    parser.add_argument('-p', '--port', metavar = 'port',
                        type = int, default = conf['port'],
                        help = 'Which port to connect with')

    parser.add_argument('-n', '--nick', metavar = 'name',
                        type = str, default = conf['nick'],
                        help = 'Nick to connect with.')

    parser.add_argument('-r', '--realname', metavar = 'realname',
                        type = str, default = conf['realname'],
                        help = 'Realname to connect with.')

    parser.add_argument('-v', '--verbose',
                        action = "store_true", default = True,
                        help = 'Turns on more verbose output.')

    args = parser.parse_args()

    try:
        os.remove(fifo_file)
        print("removed lingering tmp file.")
    except OSError:
        print("no fifo file yet.")
    os.mkfifo(fifo_file)
    with open("/tmp/ircd.fifo", 'wb') as f:
        f.write(b"hello\n")
    os.remove(fifo_file)
    quit()

def read_config(file, name):
    """Reads json configuration file"""
    with open(file, 'rt') as f:
        data = json.loads(f.read())
        conf = data[name]
        print(conf)
        return conf

def read_msg():
    pass

def send_msg():
    pass

if __name__  ==  "__main__":
    main()
