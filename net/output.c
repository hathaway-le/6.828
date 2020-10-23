#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	int32_t reqno;
	uint32_t whom;
	int perm;
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while (1)
	{
		reqno = ipc_recv((int32_t *) &whom, (void *) &nsipcbuf, &perm);

		if(reqno != NSREQ_OUTPUT)
		{
			continue;
		}
		while((reqno = sys_eth_tx((uint8_t *)nsipcbuf.pkt.jp_data,(uint32_t)nsipcbuf.pkt.jp_len))!=0)
		{
			sys_yield();
		}		
	}
	
}
