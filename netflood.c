/* Net Flood - Send as many broadcast packets as possible */
/* Written by Ian Seyler */

// Requirements: libncurses-dev
// Compile: gcc netflood.c -o netflood -Wall -lncurses

/* Global Includes */
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <fcntl.h>

/* Global defines */
#undef ETH_FRAME_LEN
#define ETH_FRAME_LEN 1518

/* Global variables */
unsigned char src_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // server address
unsigned char dst_broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
int c, s;
unsigned int count=0, lastcount=0, running=1, packet_size=128;
struct sockaddr_ll sa;
char key;
unsigned char* buffer;
struct ifreq ifr;
time_t seconds, timedelay;

/* Main code */
int main(int argc, char *argv[])
{
	printf("Net Flood - Send as many broadcast packets as possible\n");

	/* first argument needs to be a NIC */
	if (argc < 2)
	{
		printf("Please specify an Ethernet device\n");
		exit(0);
	}

	/* open a socket in raw mode */
	s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s == -1)
	{
		printf("Error: Could not open socket! Check permissions\n");
		exit(1);
	}

	/* which interface to use? */
	memset(&ifr, 0, sizeof(struct ifreq)); // sizeof(ifr)?
	strncpy(ifr.ifr_name, argv[1], IFNAMSIZ);

	/* does the interface exist? */
	if (ioctl(s, SIOCGIFINDEX, &ifr) == -1)
	{
		printf("No such interface: %s\n", argv[1]);
		close(s);
		exit(1);
	}

	/* is the interface up? */
	ioctl(s, SIOCGIFFLAGS, &ifr);
	if ((ifr.ifr_flags & IFF_UP) == 0)
	{
		printf("Interface %s is down\n", argv[1]);
		close(s);
		exit(1);
	}

	/* configure the port for non-blocking */
	if (-1 == fcntl(s, F_SETFL, O_NONBLOCK))
	{
		printf("fcntl (NonBlocking) Warning\n");
		close(s);
		exit(1);
	}

	/* get the MAC address */
	ioctl(s, SIOCGIFHWADDR, &ifr);
	for (c=0; c<6; c++)
	{
		src_MAC[c] = (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[c]; // WTF
	}

	/* just write in the structure again */
	ioctl(s, SIOCGIFINDEX, &ifr);

	/* well we need this to work */
	memset(&sa, 0, sizeof (sa));
	sa.sll_family = AF_PACKET;
	sa.sll_ifindex = ifr.ifr_ifindex;
	sa.sll_protocol = htons(ETH_P_ALL);

	initscr(); // Initialize curses
	cbreak();
	timeout(0); // Set 0 timeout for nonblocking getch()
	noecho(); // No local echo
	clear(); // Clear the screen
	curs_set(0); // Hide the cursor

	printw("This server is: %02X:%02X:%02X:%02X:%02X:%02X\n\n", src_MAC[0], src_MAC[1], src_MAC[2], src_MAC[3], src_MAC[4], src_MAC[5]);
	buffer = (void*)malloc(ETH_FRAME_LEN);

	memcpy((void*)buffer, (void*)dst_broadcast, 6);
	memcpy((void*)(buffer+6), (void*)src_MAC, 6);
	buffer[12] = 0xAB;
	buffer[13] = 0xBA;
	buffer[14] = 0x00;
	buffer[15] = 0x10;

	printw("Flooding...\n\nPress any key to exit\n");

	seconds = time(NULL);
	timedelay = seconds+1;

	while(running == 1)
	{
		seconds = time(NULL);
		if (seconds > timedelay)
		{
			move(2,12);
			printw("%d %d-byte packets - %d bytes per second", count, packet_size, (count - lastcount) * packet_size / 2);
			refresh();
			timedelay = seconds+1;
			lastcount = count;
			key = getch();
			if (key != ERR)
				running = 0;
		}
		c = sendto(s, buffer, packet_size, 0, (struct sockaddr *)&sa, sizeof (sa));
		count++;
	}

	endwin(); // Stop curses
	printf("Sent %d packets.\n", count);
	close(s);
	return 0;
}

// EOF
