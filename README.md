# netflood

Send as many broadcast packets as possible

## Compile

	gcc netflood.c -o netflood -Wall -lncurses -lpthread

## Run

	sudo ./netflood INTERFACE PACKET_COUNT

PACKET_COUNT is optional.