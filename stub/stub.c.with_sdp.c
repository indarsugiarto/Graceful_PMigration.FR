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

#define APP_SIZE				0xE700			// 0x10000 - 0x1800 - 0x100 = DTCM_SIZE - STUB_PROG_SIZE - RESERVED_ITCM_TOP_SIZE
#define RESERVED_ITCM_TOP_SIZE	0x100
#define STUB_PROG_SIZE			0x1800			// = 6144 (6KB), the current stub in aplx format
#define STUB_VAR_MEM_SIZE		0x800			// 2KB is enough?
#define DTCM_APP_BASE			0x00400800		// normal DTCM_BASE 0x00400000
#define STUB_TRIGGER_CMD		0xFA95			// magic number, just a random
#define STUB_TRIGGER_SEQ		0xBF3E

uint stubTriggerKey;
uint myCoreID;
uint itcm_addr, dtcm_addr;

void transferITCM()
{
	uint cspr = cpu_int_disable();
	uint dma_desc = DMA_TRANSFER_ITCM_ID << 26 | DMA_DWORD << 24 | DMA_BURST << 21 | 1 << 19 | APP_SIZE;

			/*  uint length: length of transfer in bytes
			*
			* OUTPUTS
			*   uint: 0 if the request queue is full, DMA transfer ID otherwise
			*
			* SOURCE
			*/
			uint spin1_dma_transfer (uint tag, void *system_address, void *tcm_address,
						 uint direction, uint length)
			{
			  uint id = 0;
			  uint cpsr = spin1_int_disable ();

			  uint new_end = (dma_queue.end + 1) % DMA_QUEUE_SIZE;

			  if (new_end != dma_queue.start)
			  {
				id = dma_id++;

				uint desc = DMA_WIDTH << 24 | DMA_BURST_SIZE << 21
				  | direction << 19 | length;

	cpu_int_restore(cspr);
}

void transferDTCM()
{

}

void dmaDone(uint tid, uint tag)
{

}

#ifdef USE_MCPL

/* - when it receives an mcpl packet containing a migration trigger, it'll start dma transfer
 * - the address arguments contains the location in sdram for the itcm and dtcm. First, it
 *   transfer the dtcm. Here, itcm_addr is contained in the "key" and the dtcm_addr is
 *   contained in the "payload".
 * */
void mcplHandler(uint itcm_addr, uint dtcm_addr)
{

}

#else
/* Use arg1 for itcm-sdram, and arg2 for dtcm-sdram
 *
 * */
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

	// get the core ID and modified so that it can fit into 1 digit hex
	myCoreID = sark_core_id();

	// register callback
#ifdef USE_MCPL
	// check router entry
	if(checkRouterEntry()==0) {
		io_printf(IO_STD, "Fail to check router\n");
		rt_error(RTE_ABORT);
	}

	// register packet buffer to be used by pkt_tx_kd()
	event_register_pkt(64, SLOT_0);
	event_register_queue (mcplHandler, EVENT_RXPKT, SLOT_1, PRIO_1);
#else
	event_register_queue (sdpHandler, EVENT_SDP, SLOT_1, PRIO_1);
#endif
	event_register_queue (dmaDone, EVENT_DMA, SLOT_FIQ, PRIO_0);

	// go, ie., sleep until it is called
	event_start (0, 0, SYNC_NOWAIT);
}


