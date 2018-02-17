# FSIRC

FSIRC is a filesystem-based IRC client written inspired by suckless "ii", but written to have a cleaner codebase, and to be easier to customize.

## Installation
First, generate a *config.h*:
``` bash
make config.h
```
Then edit it and *config.mk* to customize the program. Once both files are configured, run make:
``` bash
make clean all
sudo make install
```
The program can be uninstalled at any time with make as well:
``` bash
make uninstall
```

## Running FSIRC
To run the FSIRC client, simply run the executable with the first argument being the hostname of the irc server to connect to, like so:
``` bash
fsirc chat.freenode.net
```
This will, by default, create a directory "/tmp/fsirc/chat.freenode.net/" with two files for input and output to the server. The two files are "in" to pipe input to, and "out" for FSIRC to log output to.

## Sending commands to the client
FSIRC communicates directly through FIFO file communication. For example, joining a channel is as simple as echoing text to the "in" file:
``` bash
echo '/j #mychannel' > /tmp/fsirc/chat.freenode.net/in
```
This will create a new channel directory under the path
"/tmp/fsirc/chat.freenode.net/#mychannel" with an "in" and an "out" file for the channel, which can be used to communicate with the channel rather than the server.

Writing a 'me' message:
``` bash
echo '/m uses FSIRC' > /tmp/fsirc/chat.freenode.net/#mychannel/in
```
Leaving a channel:
``` bash
echo '/p #mychannel' > /tmp/fsirc/chat.freenode.net/in
```
To overcome the limitations of my own laziness since I haven't programmed in
all of the irc commands yet, there is a way to send 'raw' irc protocol in messages to in case a command is not supported by the client yet:
``` bash
echo '/r PING bob' > /tmp/yorha/chat.freenode.net/in
```

## Dependencies
- C99 compliant compiler

## Note
Feel free to submit pull requests here, or send me an email if you would rather not use github.
