Yorha is a filesystem-based IRC client written in C inspired by suckless "ii",
but written to have a cleaner, faster, and more customizable codebase.

## Installation
First, customize any settings available in 'src/config.h' and 'config.mk' to your liking.
Second running the following commands:
``` bash
make clean all
sudo make install
```

## Running Yorha
To run the yorha irc client, simply run the binary file with the first argument
being the name of the irc server to connect to, like so:
``` bash
./yorha chat.freenode.net
```
This will, by default, create a directory "/tmp/yorha/chat.freenode.net/" with
a FIFO file name "in" and an output file named "out" inside of it.

## Sending commands to the client
Yorha communicates directly through FIFO file communication. For example,
joining a channel is as simple as writing text to the "in" file:
``` bash
echo '/j #mychannel' > /tmp/yorha/chat.freenode.net/in
```
This whill create a new channel directory under the path
"/tmp/yorha/chat.freenode.net/#mychannel" with an "in" and an "out" file.
Writing a 'me' message:
``` bash
echo '/m uses yorha irc client' > /tmp/yorha/chat.freenode.net/#mychannel/in
```
Leaving a channel:
``` bash
echo '/p #mychannel' > /tmp/yorha/chat.freenode.net/in
```
To overcome the limitations of my own laziness since I haven't programmed in
all of the irc commands yet, there is a way to send 'raw' irc protocol to
servers in case a command is not supported by the client yet:
``` bash
echo '/r JOIN #mychannel' > /tmp/yorha/chat.freenode.net/in
```

## Dependencies
- C99 compliant compiler
- libstx (www.toddgaunt.com) (https://www.github.com/toddgaunt/libstx)

## Note
Let me know if you like this "ii" rewrite and I'll be more likely to update and
work on it!
