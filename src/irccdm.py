#! /usr/bin/env python3
# This program is the commandline tool for interacting with ircd.
# It will allow you to control ircd from the cmd line.

import argparse
import os
import json
import sys
import socket

def main():
    parser = argparse.ArgumentParser(description = "Messaging client for irccd  \
                                                    The commands are:   \
                                                        write   \
                                                            [message,\
                                                            nick],    \
                                                        host    \
                                                            [add, \
                                                            remove,  \
                                                            connect, \
                                                            disconnect,  \
                                                            ping],    \
                                                        channel \
                                                            [add, \
                                                            remove,  \
                                                            join,    \
                                                            part,    \
                                                            list]    \
                                                    ")
    parser.add_argument ('command', metavar='command',
                        nargs='+',
                        help='Subcommand to be run')

    parser.add_argument ('-p', '--profile', metavar='profile',
                        type=str, default="default",
                        help='Which profile to load for connection info')

    parser.add_argument ('-c', '--config-file', metavar='filepath',
                        type=str, default="profiles.json",
                        help='Specify a profile file to load')

    parser.add_argument ('-s', '--sockpath', metavar='sockpath',
                        type=str, default='/tmp/irccd.socket',
                        help='Path to unix socket')

    args = parser.parse_args(sys.argv[1:2])

    if args.command[0] == "write":
        cmd_write(args)
    elif args.command[0] == "host":
        cmd_host(args)
    elif args.command[0] == "channel" or args.command[0] == "chan":
        cmd_channel(args)
    elif args.command[0] == "quit":
        sock = socket_connect(args.sockpath)
        socket_send(sock, 'Q')
        print(cstr(socket_recv(sock)))
    else:
        print("No valid commands entered")
        quit()

def cmd_write(args):
    conf = read_config(args.config_file, args.profile)

    parser = argparse.ArgumentParser(description = "Write messages to irc")
    parser.add_argument('command', metavar='command',
                        type = str, default = '',
                        help = 'Subcommand to be run [message | msg]')

    parser.add_argument('-m', '--message', metavar = 'message',
                        type = str,
                        help = 'Message to be sent')

    write_args = parser.parse_args(sys.argv[2:])
    command = write_args.command

    if write_args.message:
        msg = write_args.message
    else:
        #TODO open up default editor so user can write a message
        msg = ""

    if command == "message" or command == "msg":
        sock = socket_connect(args.sockpath)
        socket_send(sock, 'w' + msg)
        print(cstr(socket_recv(sock)))
    else:
        invalid_cmd()

def cmd_host(args):
    conf = read_config(args.config_file, args.profile)

    parser = argparse.ArgumentParser(description = "Manage connection to host")
    parser.add_argument('command', metavar='command',
                        type = str, default = '',
                        help = 'Subcommand to be run [add], [remove], \
                                [connect], [disconnect], [ping]')

    parser.add_argument('-H', '--host', metavar='host',
                        type = str,
                        help = 'Host ip or name')

    host_args = parser.parse_args(sys.argv[2:])
    command = host_args.command

    if host_args.host:
        host = host_args.host
        if host in conf["saved_hosts"]:
            host = conf["saved_hosts"][host]
    else:
        host = conf["default_host"]

    if command == "add":
        #TODO adds host to hostdict
        pass
    elif command == "remove":
        #TODO
        pass
    elif command == "connect":
        sock = socket_connect(args.sockpath)
        socket_send(sock, 'c' + host)
        print(cstr(socket_recv(sock)))
    elif command == "disconnect":
        sock = socket_connect(args.sockpath)
        socket_send(sock, 'd')
        print(cstr(socket_recv(sock)))
    elif command == "ping":
        sock = socket_connect(args.sockpath)
        socket_send(sock, 'P')
        print(cstr(socket_recv(sock)))
    else:
        invalid_cmd()

def cmd_channel(args):
    conf = read_config(args.config_file, args.profile)
    parser = argparse.ArgumentParser(description = "Manage irc channels")

    parser.add_argument('command', metavar='command',
                        type = str, default = '',
                        help = 'Subcommand to be run [add], [remove], [list], [join], [part]')

    parser.add_argument('-c', '--channel', metavar='channel',
                        type = str,
                        help = 'Target Channel')

    parser.add_argument('-s', '--saved-channel', metavar='channel',
                        type = str,
                        help = 'Target Channel')

    chan_args = parser.parse_args(sys.argv[2:])
    command = chan_args.command

    if chan_args.channel:
        channel = chan_args.channel
    else:
        channel = ""

    if isinstance(channel, int):
        if channel >= 0 and channel < len(conf["saved_channels"]):
            channel = conf["saved_channels"][channel]

    if command == "add":
        #TODO
        pass
    elif command == "remove":
        #TODO
        pass
    elif command == "joined":
        sock = socket_connect(args.sockpath)
        socket_send(sock, 'C')
        print("Joined: " + cstr(socket_recv(sock)))
    elif command == "join":
        sock = socket_connect(args.sockpath)
        socket_send(sock, 'j' + channel)
        print(cstr(socket_recv(sock)))
    elif command == "part":
        sock = socket_connect(args.sockpath)
        socket_send(sock, 'p' + channel)
        print(cstr(socket_recv(sock)))
    else:
        invalid_cmd()

def socket_connect(sockpath):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(sockpath)
    return sock

def socket_recv(sock):
    try:
        msg = sock.recv(512)
        sock.close()

        return msg
    except OSError:
        print ("No socket connection")

def socket_send(sock, msg):
    msg = str.encode(msg)
    try:
        sock.send(msg)
    except OSError:
        print ("No socket connection.")

def read_config(file, name):
    """Reads json configuration file"""
    with open(file, 'rt') as fd:
        data = json.loads(fd.read())
        conf = data[name]
        return conf

def invalid_cmd():
    print ("Not a valid subcommand")
    quit()

def cstr(f):
    """ Converts byte-strings from C to UTF-8 strings """
    f = f.decode("utf-8")
    f = f.replace('\n', '')
    return f

if __name__  ==  "__main__":
    main()
