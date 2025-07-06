/* Net Flood - Send as many broadcast packets as possible */
/* Written by Ian Seyler */

// Requirements: libncurses-dev
// Compile: gcc netflood.c -o netflood -Wall -lncurses -lpthread

// sudo ./netflood INTERFACE PACKET_COUNT
// PACKET_COUNT is optional.

/* Global Includes */
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <locale.h>
#include <pthread.h>

/* Global defines */
#undef ETH_FRAME_LEN
#define ETH_FRAME_LEN 1518
#define NUM_THREADS 1

/* Global variables */
unsigned char src_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // server address
unsigned char dst_broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
int c, s, seconds_to_run;
unsigned int msec=0, lastcount=0, running=1, packet_size=1500, packets_to_send;
unsigned long long iterations=0, count=0, errors=0;
struct sockaddr_ll sa;
char key;
unsigned char* buffer;
struct ifreq ifr;
time_t seconds, timedelay;

/* Global functions */
void* send_packets(void* arg);

/* Main code */
int main(int argc, char *argv[])
{
	setlocale(LC_NUMERIC, "");

	/* first argument needs to be a NIC */
	if (argc < 2)
	{
		printf("Please specify an Ethernet device\n");
		exit(0);
	}

	if (argc == 3)
	{
		if (sscanf (argv[2], "%u", &packets_to_send) != 1)
		{
			printf("Specify a valid number of packets to send\n");
			exit(0);
		}
	}
	else
	{
		packets_to_send = 0; // Infinite packets
	}

	/* open a socket in raw mode */
	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
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

	printw("Flooding...\n\nPress any key to exit");

	time_t start = time(NULL);

	pthread_t threads[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; i++)
	{
		if (pthread_create(&threads[i], NULL, send_packets, NULL) != 0) {
			perror("Thread creation failed");
			exit(EXIT_FAILURE);
		}
	}

	while(running == 1)
	{
		key = getch();
		if (key != ERR)
			running = 0;
	}

	time_t end = time(NULL);

	endwin(); // Stop curses

	unsigned int seconds = end - start;
	unsigned long long data_sent = count * packet_size;
	unsigned int data_send_MB = data_sent / 1048576;

	printf("%12d seconds elapsed", seconds);
	printf("\n%'12lld iterations\n%'12lld packets send\n%'12lld TX errors\n%'12d MiB transferred", iterations, count, errors, data_send_MB);
	if (seconds > 0)
		printf("\n%'12d MiB/s (%'d Mbps)", data_send_MB / seconds, (data_send_MB * 8) / seconds);
	printf("\n");
	close(s);
	return 0;
}

void* send_packets(void* arg)
{
	while(running == 1)
	{
		memcpy((void*)(buffer+16), (void*)&count, 8);
		c = sendto(s, buffer, packet_size, 0, (struct sockaddr *)&sa, sizeof(struct sockaddr_ll));

		if (c == packet_size)
		{
			count++; // Increment the count of packets that were sent successfully
			if (count == packets_to_send)
				running = 0;
		}
		else
		{
			errors++;
		}

		iterations++;
	}

	pthread_exit(NULL);
}

// EOF

