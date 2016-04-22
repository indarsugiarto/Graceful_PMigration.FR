/* TODO:
 * 1. Make sure trigger.aplx runs first and after app. 1 second, load the pingpong
 *    The stub.aplx should also run before trigger.aplx
 *
 * */

#include <spin1_api.h>
#include "../include/pmigration.h"
/* for demonstration only */

sdp_msg_t *msg;
uint myCoreID;
app_stub_t as[MAX_AVAIL_CORE];	// in reality, we have max 15 cores running in a chip, because 1 core is a spare

void timeout(uint tick, uint null);

void initAppStub()
{
	uchar i;
	for(i=0; i<MAX_AVAIL_CORE; i++) as[i].coreID = i+1;
}

/* SYNOPSIS
 *		allocateTCM() will allocate space in sdram to make a storage for a TCM's application
 *
 * FUTURE:
 *		- if app is exit, then deallocate the sdram
 * */
void allocateTCM(uint core, uint appID)
{
	//for debugging
	io_printf(IO_BUF, "Will allocate TCMSTG for core-%d...\n", core);

	// first, allocate TCM storage in sdram and tag its purpose
	// sark_xalloc will return a pointer to allocated block or NULL on failure
	uint *sdram_addr = (uint *)sark_xalloc(sv->sdram_heap, APP_TCM_SIZE, core, ALLOC_LOCK);
	if(sdram_addr==NULL) {
		io_printf(IO_BUF, "Fatal Error: SDRAM allocation for TCM storage!\n");
		rt_error(RTE_ABORT);
	}

	// just for debugging
	io_printf(IO_BUF, "Allocating TCMSTD at 0x%x\n", sdram_addr);

	as[core-1].tcm_addr = sdram_addr;

	// then send notification to requesting app
	uint key = KEY_SUPV_SEND_TCMSTG | (core << 24);
	uint payload = (uint)as[core-1].tcm_addr;
	io_printf(IO_BUF, "Supposed to send MCPL with key 0x%x and payload 0x%x\n", key, payload);
	spin1_send_mc_packet(key, payload, 1);
}

/* SYNOPSIS:
 *		supv-core is responsible to allocate keys for all app- and stub-cores
 *		benefit: stub-core doesn't need to allocate it manually
 * */
void initRouter()
{
	uint i, e = rtr_alloc(17);	// allocate all 17 cores, excluding the monitor core-0
	if(e != 0) {
		for(i=1; i<=17; i++)
			rtr_mc_set(e+(i-1), (i << 24), MASK_CORE_SELECTOR, (1 << (6 + i)));
	}
	else {
		rt_error(RTE_ABORT);
	}
}

void hSDP(uint mBox, uint port)
{

}



void c_main(void)
{
	myCoreID = sark_core_id();
	uint myAppID = sark_app_id();
	// put to core-17 to mimic my future supervisory program
	if(myCoreID != 17 || myAppID != SUPERVISOR_APP_ID) {
		io_printf(IO_STD, "Put me in core-17 with ID-255 please!\n");
		rt_error(RTE_ABORT);
	}
	// init appstub ("as") to hold all stubs' data
	initAppStub();

	initRouter();

	spin1_callback_on(SDP_PACKET_RX, hSDP, 0);

	// for demonstration, we use timer to stimulate external trigger for migration
	spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
	// Let's trigger the demo from here. After several seconds (see timeout()), it'll start...
	spin1_start(SYNC_NOWAIT);
}

