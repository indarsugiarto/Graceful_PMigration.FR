/* SYNOPSIS
 *		Stub will stay somewhere at higher ITCM and response to a call via MCPL to transfer
 *		code from SDRAM to lower partition of ITCM and DTCM.
 *
 *
 * Rev:
 * 16.11.2015
 * - Regarding mcpl and sdram addressing mechanism, since the SDRAM address IS within 0x6...
 *   then we can simplify by sending only the 7-digit address (omitting MSD, ie. 0x6).
 *   Idea: itcm addr in SDRAM will be embedded in the key, and dtcm addr in SDRAM will be
 *   embedded in the payload. But then, one digit of the itcm part will be moved into the
 *   payload part. Here is the schema:
 *                  key                          payload
 *   [Core-ID, itcm-in-sdram(6:1)] [itcm-in-sdram(0:0), dtcm-in-sdram(6:0)]
 * */

#include <sark.h>

#define STUB_APP_ID			254
#define STUB_TRIGGER_KEY	0x12345600		// easy number

#define DMA_ID				14				// magic number. Note, the DMA transfer ID is only 6-bits
#define DMA_DWORD			1				// 0 = word, 1 = double word
#define DMA_BURST			16				// valid values: 1, 2, 4, 8, 16. So, let's try as fast as possible

#define DEBUG_LEVEL			0				// 0 = no io_printf, 1 = with io_printf

uint stubTriggerKey;
uint myCoreID;

void transferDTCM(uint address)
{

}

void transferITCM(uint address)
{

}

void dmaDone(uint tid, uint tag)
{

}

/* - when it receives an mcpl packet containing a migration trigger, it'll start dma transfer
 * - the address arguments contains the location in sdram for the itcm and dtcm. First, it
 *   transfer the dtcm. Here, itcm_addr is contained in the "key" and the dtcm_addr is
 *   contained in the "payload".
 * */
void mcplHandler(uint itcm_addr, uint dtcm_addr)
{

}

// check if the routing to my core is already exist
uint checkRouterEntry()
{
	uint i, e = rtr_alloc(1);
	//uint keyToMe = myCoreID | STUB_TRIGGER_KEY;
	uint keyToMe = myCoreID << 24;
	uint found = 0;
	if(e!=0) {
		// first, check if my entry is already there
		rtr_entry_t *r;
		for(i=0; i<e; i++){
			rtr_mc_get(i, r);
			if(r->key==keyToMe){
				found = 1;
				break;
			}
		}
		// if the key that goes to me doesn't exist, create one
		if(found==0){
			rtr_mc_set(e, keyToMe, 0xFF000000, (1 << (6+myCoreID)));
		}
	}
	return e;
}

void c_main(void)
{
	// check app-ID
	if(sark.vcpu->app_id != STUB_APP_ID) {
#if(DEBUG_LEVEL==1)
		io_printf(IO_STD, "Please assign me app-ID %u!\n", STUB_APP_ID);
#endif
		return;
	}

	// get the core ID and modified so that it can fit into 1 digit hex
	myCoreID = sark_core_id();

	// check router entry
	if(checkRouterEntry()==0) {
		io_printf(IO_STD, "Fail to check router\n");
		rt_error(RTE_ABORT);
	}

	// register packet buffer to be used by pkt_tx_kd()
	event_register_pkt(64, SLOT_0);

	// register callback
	event_register_queue (mcplHandler, EVENT_RXPKT, SLOT_1, PRIO_1);
	event_register_queue (dmaDone, EVENT_DMA, SLOT_FIQ, PRIO_0);

	// go, ie., sleep until it is called
	event_start (0, 0, SYNC_NOWAIT);
}


