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
 * 17.11.2015
 * - Let's use sdp, because MCPL keys may be used by the application
 * */

#include <sark.h>

#define USE_MCPL

#define STUB_APP_ID				254
#define STUB_SDP_PORT			7

#define DMA_TRANSFER_ITCM_ID	14				// magic number. Note, the DMA transfer ID is only 6-bits
#define DMA_TRANSFER_DTCM_ID	41
#define DMA_DWORD				1				// 0 = word, 1 = double word
#define DMA_BURST				16				// valid values: 1, 2, 4, 8, 16. So, let's try as fast as possible

#define DEBUG_LEVEL				0				// 0 = no io_printf, 1 = with io_printf

#define RESERVED_ITCM_TOP_SIZE	0x100
#define STUB_PROG_SIZE			0x1800			// = 6144 (6KB), the current stub in aplx format
#define STUB_DATA_SIZE			0x800			// 2KB is enough?
#define DTCM_APP_BASE			0x00400800		// normal DTCM_BASE 0x00400000

#define APP_PROG_SIZE			0x6700			// 0x8000 - 0x1800 - 0x100 = ITCM_SIZE - STUB_PROG_SIZE - RESERVED_ITCM_TOP_SIZE
#define APP_DATA_SIZE			0xF800			// DTCM_SIZE - STUB_DATA_SIZE = 0x10000 - 0x800

#define STUB_TRIGGER_CMD		0xFA95			// magic number, just a random
#define STUB_TRIGGER_SEQ		0xBF3E

uint stubTriggerKey;
uint myCoreID;
uint itcm_addr, dtcm_addr;

void transferITCM()
{
	uint cspr = cpu_int_disable();
	//transfering from TCM to SDRAM use: uint dma_desc = DMA_TRANSFER_ITCM_ID << 26 | DMA_DWORD << 24 | DMA_BURST << 21 | 1 << 19 | APP_PROG_SIZE;
	uint dma_desc = DMA_TRANSFER_ITCM_ID << 26 | DMA_DWORD << 24 | DMA_BURST << 21 | APP_PROG_SIZE;		// we're reading from SDRAM!!!
	dma[DMA_ADRS] = itcm_addr;
	dma[DMA_ADRT] = ITCM_BASE;
	dma[DMA_DESC] = dma_desc;
	cpu_int_restore(cspr);
}

void transferDTCM()
{
	uint cspr = cpu_int_disable();
	uint dma_desc = DMA_TRANSFER_DTCM_ID << 26 | DMA_DWORD << 24 | DMA_BURST << 21 | APP_DATA_SIZE;
	dma[DMA_ADRS] = dtcm_addr;
	dma[DMA_ADRT] = DTCM_APP_BASE;
	dma[DMA_DESC] = dma_desc;
	cpu_int_restore(cspr);
}

void dmaDone(uint tid, uint tag)
{

}

#ifdef USE_MCPL
// check if the routing to my core is already exist
void checkRouterEntry()
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
	else {
		io_printf(IO_STD, "Fail to check router\n");
		rt_error(RTE_ABORT);
	}

}

/* - when it receives an mcpl packet containing a migration trigger, it'll start dma transfer
 * - the address arguments contains the location in sdram for the itcm and dtcm. First, it
 *   transfer the dtcm. Here, a part of itcm_addr is contained in the "key" and the dtcm_addr is
 *   mainly contained in the "payload". See project description on top of this file:
 *                  key                          payload
 *   [Core-ID, itcm-in-sdram(6:1)] [itcm-in-sdram(0:0), dtcm-in-sdram(6:0)]
 * */
void mcplHandler(uint key, uint payload)
{
	itcm_addr = (key & 0x00FFFFFF) << 8;				// discard the CoreID and get 0x0FFFFFF0
	itcm_addr |= (payload >> 28) | 0x60000000;
	dtcm_addr = (payload & 0x0FFFFFFF) | 0x60000000;
	transferITCM();
}

#else
/* Use arg1 for itcm-sdram, and arg2 for dtcm-sdram */
void sdpHandler(uint mailbox, uint port)
{
	sdp_msg_t *msg = sark_msg_get();
	uint triggered = 0;

	// check if sent by core 17 via port STUB_SDP_PORT
	if(msg->cmd_rc==STUB_TRIGGER_CMD && msg->seq==STUB_TRIGGER_SEQ) {
		itcm_addr = msg->arg1;
		dtcm_addr = msg->arg2;
		triggered = 1;
	}

	sark_msg_free (msg);
	if(triggered)
		transferITCM();
}
#endif

void c_main(void)
{
	// check app-ID
	if(sark.vcpu->app_id != STUB_APP_ID) {
#if(DEBUG_LEVEL==1)
		io_printf(IO_STD, "Please assign me app-ID %u!\n", STUB_APP_ID);
#endif
		return;
	}

	myCoreID = sark_core_id();

	// register callbacks
#ifdef USE_MCPL
	checkRouterEntry();					// check router entry
	event_register_pkt(64, SLOT_0);		// register packet buffer to be used by pkt_tx_kd()
	event_register_queue (mcplHandler, EVENT_RXPKT, SLOT_1, PRIO_1);
#else
	event_register_queue (sdpHandler, EVENT_SDP, SLOT_1, PRIO_1);
#endif

	// prepare dma
	dma[DMA_CTRL] = 0x3f;				// Abort pending and active transfers
	dma[DMA_CTRL] = 0x0d;				// clear possible transfer done and restart
	dma[DMA_GCTL] = 0x000c00;			// enable dma done interrupt
	event_register_queue (dmaDone, EVENT_DMA, SLOT_FIQ, PRIO_0);

	// go, ie., sleep until it is called
	event_start (0, 0, SYNC_NOWAIT);
}


