#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>


int main(int argc,char **argv)
{
	int s;
	int nbytes;
	struct sockaddr_can addr;
	struct can_frame frame;
	struct ifreq ifr;

	if (argc != 2)
	{
		printf("usage : sendCAN <CAN interface>\n");
		printf("ex. sendCAN can1\n");
		return 0;
	}

	char *ifname = argv[1];
 
	if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
	{
		perror("Error while opening socket");
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ioctl(s, SIOCGIFINDEX, &ifr);
	
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex; 


	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
	{
		perror("Error in socket bind");
		return -2;
	}

	frame.can_id  = 0x123;
	frame.can_dlc = 8;
	frame.data[0] = 0x1;
	frame.data[1] = 0x2;
	frame.data[2] = 0x3;
	frame.data[3] = 0x4;
	frame.data[4] = 0x5;
	frame.data[5] = 0x6;
	frame.data[6] = 0x7;
	frame.data[7] = 0x8;				

	printf("using %s to write\n", ifname);
	nbytes = write(s, &frame, sizeof(struct can_frame));

	return 0;
}
