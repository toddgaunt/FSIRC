#! /bin/bash

urxvt -e ./irccd &

./irccdm.py host connect -H '162.213.39.42'

./irccdm.py chan join -c '#Y35chan'
sleep 20
./irccdm.py write message -m 'hello'
sleep 1
./irccdm.py write message -m 'this is a test'
./irccdm.py write message -m 'this is a test'
sleep 1
./irccdm.py chan part -c '#Y35chan'
sleep 1
./irccdm.py chan part -c '#Y35chan'
sleep 1
./irccdm.py chan join -c '#Y35chan'
sleep 1
./irccdm.py chan join -c '#Y35chan'
sleep 1
./irccdm.py chan join -c '#Y35chan'
sleep 1
./irccdm.py chan join -c '#Y35chan'
sleep 1
./irccdm.py write message -m 'this is a test'
sleep 1
./irccdm.py host connect -H '162.213.39.42'
sleep 1
./irccdm.py host connect -H '162.213.39.42'
sleep 1
./irccdm.py host disconnect
sleep 10
./irccdm.py quit
