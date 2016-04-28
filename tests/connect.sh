#! /bin/bash

urxvt -e ./build/irccd &

./src/irccdm.py host connect -H '162.213.39.42'

./src/irccdm.py chan join -c '#Y35chan'
sleep 20
./src/irccdm.py write message -m 'hello'
sleep 1
./src/irccdm.py write message -m 'this is a test'
./src/irccdm.py write message -m 'this is a test'
sleep 1
./src/irccdm.py chan part -c '#Y35chan'
sleep 1
./src/irccdm.py chan part -c '#Y35chan'
sleep 1
./src/irccdm.py chan join -c '#Y35chan'
sleep 1
./src/irccdm.py chan join -c '#Y35chan'
sleep 1
./src/irccdm.py chan join -c '#Y35chan'
sleep 1
./src/irccdm.py chan join -c '#Y35chan'
sleep 1
./src/irccdm.py write message -m 'this is a test'
sleep 1
./src/irccdm.py host connect -H '162.213.39.42'
sleep 1
./src/irccdm.py host connect -H '162.213.39.42'
sleep 1
./src/irccdm.py host disconnect
sleep 10
./src/irccdm.py quit
