#! /usr/bin/env python3
# This program is the commandline tool for interacting with ircd.
# It will allow you to control ircd from the cmd line.

import argparse
import os
import json
import time
import sys

def main():
    fifo_file = "/tmp/irccd.fifo"
    conf = read_config('config.json', 'default')

    parser = argparse.ArgumentParser(description = "Simple python based irc-bot daemon")

    parser.add_argument('command', metavar='command',
                        nargs = '+',
                        help = 'Subcommand to be run')

    args = parser.parse_args(sys.argv[1:2])

    if args.command[0] == "write":
        msg = cmd_write()
    elif args.command[0] == "host":
        msg = cmd_host()
    elif args.command[0] == "channel" or args.command[0] == "chan":
        msg = cmd_channel()
    else:
        print("No commands entered")
        quit()
    try:
        fd = open(fifo_file, 'wb', 0)
        fd.write(msg)
        fd.close()
    except OSError:
        print ("No fifo file")

def read_config(file, name):
    """Reads json configuration file"""
    with open(file, 'rt') as fd:
        data = json.loads(fd.read())
        conf = data[name]
        return conf

def cmd_write():
    parser = argparse.ArgumentParser(description = "Write messages to irc")

    parser.add_argument('command', metavar='command',
                        type = str, default = '',
                        help = 'Subcommand to be run')

    parser.add_argument('-m', '--message', metavar = 'message',
                        type = str,
                        help = 'Message to be sent')

    args = parser.parse_args(sys.argv[2:])
    command = args.command

    if args.message:
        msg = args.message
    else:
        #TODO open up default editor so user can write a message
        msg = ""

    if command == "message" or command == "msg":
        msg = 'w' + msg
    else:
        invalid()

    return str.encode(msg)

def cmd_host():
    parser = argparse.ArgumentParser(description = "Manage connection to host")

    parser.add_argument('command', metavar='command',
                        type = str, default = '',
                        help = 'Subcommand to be run')

    parser.add_argument('-H', '--host', metavar='host',
                        type = str,
                        help = 'Host ip or name')

    args = parser.parse_args(sys.argv[2:])
    command = args.command

    if args.host:
        host = args.host
    else:
        host = ""

    if command == "add":
        #TODO adds host to hostdict
        pass
    elif command == "remove":
        #TODO
        pass
    elif command == "connect":
        msg = 'c' + host
    elif command == "disconnect":
        msg = 'd'
    elif command == "ping":
        msg = 'p'
    else:
        invalid()

    return str.encode(msg)

def cmd_channel():
    parser = argparse.ArgumentParser(description = "Manage irc channels")

    parser.add_argument('command', metavar='command',
                        type = str, default = '',
                        help = 'Subcommand to be run')

    parser.add_argument('-c', '--channel', metavar='channel',
                        type = str,
                        help = 'Target Channel')

    args = parser.parse_args(sys.argv[2:])
    command = args.command

    if args.channel:
        channel = args.channel
    else:
        channel = ""

    if command == "add":
        #TODO
        pass
    elif command == "remove":
        #TODO
        pass
    elif command == "join":
        msg = 'j' + channel
    elif command == "part":
        msg = 'p' + channel
    else:
        invalid()

    return str.encode(msg)

def invalid_cmd():
        print ("Not a valid subcommand")
        quit()

if __name__  ==  "__main__":
    main()
