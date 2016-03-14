#! /usr/bin/env python3
# This program is the commandline tool for interacting with ircd.
# It will allow you to control ircd from the cmd line.
#TODO allow for commands to be looped infinitely with an argument, eg. send READ until break

import argparse
import os
import json
import time

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

    parser.add_argument('-P', '--port', metavar = 'port',
                        type = int, default = conf['port'],
                        help = 'Which port to connect with')

    parser.add_argument('-n', '--nick', metavar = 'name',
                        type = str, default = conf['nick'],
                        help = 'Nick to connect with.')

    parser.add_argument('-w', '--write',
                        type = str,
                        help = 'Write a line of text to irc.')

    parser.add_argument('-r', '--read',
                        action = 'store_true', default = False,
                        help = 'Read the next line of text on irc.')

    parser.add_argument('-R', '--Realname', metavar = 'realname',
                        type = str, default = conf['realname'],
                        help = 'Realname to connect with.')

    parser.add_argument('-v', '--verbose',
                        action = "store_true", default = True,
                        help = 'Turns on more verbose output.')

    args = parser.parse_args()

    os.mkfifo(fifo_file)
    fd = open(fifo_file, 'wb', 0)
    if args.read:
        msg = b'r' + str.encode(args.read) # byte r = 'read' mode
    elif args.write:
        msg = b'w' + str.encode(args.write) # byte w = 'write' mode
        print(msg)
    elif args.join:
        msg = b'j' + str.encode(args.join) # byte # = 'chan' mode
    else:
        pass
        #msg = b'e' # e = 'error' mode
    fd.write(msg)
    fd.close()

def read_config(file, name):
    """Reads json configuration file"""
    with open(file, 'rt') as fd:
        data = json.loads(fd.read())
        conf = data[name]
        return conf

def read_msg():
    pass

def write_msg():
    pass

def open_pipe():
    pass

if __name__  ==  "__main__":
    main()
