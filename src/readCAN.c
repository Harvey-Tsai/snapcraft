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
#include <linux/can/error.h>
#include <fcntl.h>



int main(int argc,char **argv)
{
	int s,i;
	int nbytes;
	struct sockaddr_can addr;
	struct can_frame frame;
	struct ifreq ifr;
	char *ifname = argv[1];

	int setflag,getflag,ret =0;
	
	if (argc != 2)
	{
		printf("usage : readCAN <CAN interface>\n");
		printf("ex. readCAN can0\n");
		return 0;
	}
 
	if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("Error while opening socket");
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ioctl(s, SIOCGIFINDEX, &ifr);


	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex; 

	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Error in socket bind");
		return -2;
	}

	
	setflag = setflag|O_NONBLOCK;
	ret = fcntl(s,F_SETFL,setflag);
	getflag = fcntl(s,F_GETFL,0);

	can_err_mask_t err_mask = CAN_ERR_TX_TIMEOUT |CAN_ERR_BUSOFF;
	ret = setsockopt(s, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask));
	if (ret!=0)
		printf("setsockopt fail\n");

	printf("using %s to read\n", ifname);

	while(1)
	{
		nbytes = read(s, &frame, sizeof(frame));
		if (nbytes > 0)
		 {
		   if (frame.can_id & CAN_ERR_FLAG)
		   	printf("error frame\n");
		   else
			{
			   printf("\nID=0x%x DLC=%d \n",frame.can_id,frame.can_dlc);
			   for (i =0;i<frame.can_dlc;i++)
				{
			  	  printf("data[%d]=0x%x \n",i,frame.data[i]);
				}
			}

		 }
	}

	
	return 0;
}
