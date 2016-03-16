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
                        type = str,
                        help = 'Host to connect to.')

    parser.add_argument('-j', '--join', metavar = 'channel',
                        type = str,
                        help = 'Channel to connect to.')

    parser.add_argument('-p', '--part', metavar = 'channel',
                        type = str,
                        help = 'Channel to part with.')

    parser.add_argument('-P', '--port', metavar = 'port',
                        type = int,
                        help = 'Which port to connect with')

    parser.add_argument('-n', '--nick', metavar = 'name',
                        type = str,
                        help = 'Nick to connect with.')

    parser.add_argument('-w', '--write',
                        nargs = '+', type = str,
                        help = 'Write a line of text to irc.')

    parser.add_argument('-q', '--quit',
                        action = 'store_true', default = False,
                        help = 'Make the server quit.')

    parser.add_argument('-r', '--read',
                        action = 'store_true', default = False,
                        help = 'Read the next line of text on irc.')

    parser.add_argument('-R', '--Realname', metavar = 'realname',
                        type = str,
                        help = 'Realname to connect with.')

    parser.add_argument('-v', '--verbose',
                        action = "store_true", default = True,
                        help = 'Turns on more verbose output.')

    args = parser.parse_args()

    try:
        fd = open(fifo_file, 'wb', 0)
    except OSError:
        print ("server not up.")
    if args.write:
        for i in range(len(args.write)):
            msg = b'w' + str.encode(args.write[i]) # byte w = 'write' mode
            fd.write(msg)
            print(msg)
    if args.part:
        msg = b'p' + str.encode(args.part) # byte p = 'part' mode
        fd.write(msg)
        print(msg)
    if args.join:
        msg = b'j' + str.encode(args.join) # byte j = 'join' mode
        fd.write(msg)
        print(msg)
    if args.read:
        msg = b'r'
        fd.write(msg)
        print(msg)
    if args.quit:
        msg = b'q'
        fd.write(msg)
        print(msg)
    else:
        pass
        #msg = b'e' # e = 'error' mode
    fd.close()

def read_config(file, name):
    """Reads json configuration file"""
    with open(file, 'rt') as fd:
        data = json.loads(fd.read())
        conf = data[name]
        return conf

if __name__  ==  "__main__":
    main()
