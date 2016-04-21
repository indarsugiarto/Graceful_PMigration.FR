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
 * - THINK: use sdp, because MCPL keys may be used by the application. Using sdp, the
 *   following payload can be used:
 *   arg1 = itcm address in sdram
 *   arg2 = dtcm address in sdram
 *   arg3 = restart address
 * - Let's use api to handle interrupt properly
 * - Modify key & payload allocation of MCPL. Here is the new schema:
 *                  key                          payload
 *   [Core-ID(7:6), where-to-start(5:0)] [tcm-in-sdram(7:0)]
 * 20.11.2015
 * - Remove checkRouterEntry(), since it is handled by supv-core
 * */

#include <sark.h>
#include "../pmigration.h"

#define USE_MCPL_OR_SDP			USE_MCPL
#define USE_SARK_OR_API			USE_API


uint stubTriggerKey;
uint myCoreID;
uint myTick;
uint dtcm_addr = 0;
uint itcm_addr = 0;
uint ajmp_addr = 0;

void dmaDoneHandler(uint tid, uint tag)
{
	// jump to app-addr
}


void sdpHandler(uint mbox, uint port)
{
	sdp_msg_t *sdp_msg = (sdp_msg_t *) mbox;
	uint addr[3];
	sark_mem_cpy((void *)addr, (void *)sdp_msg->data, sizeof(uint)*3);
	itcm_addr = addr[0];
	dtcm_addr = addr[1];
	ajmp_addr = addr[2];

#if(DEBUG_LEVEL==1)
	if(sdp_msg->cmd_rc==STUB_TRIGGER_CMD && port==STUB_SDP_PORT) {
		io_printf(IO_BUF, "Core-%d receives a message in port-%d:\n", myCoreID, port);
		io_printf(IO_BUF, "seq = %d\n", sdp_msg->seq);
		io_printf(IO_BUF, "APP_ITCM_ADDR = 0x%x\n", addr[0]);
		io_printf(IO_BUF, "APP_DTCM_ADDR = 0x%x\n", addr[1]);
		io_printf(IO_BUF, "APP_AJMP_ADDR = 0x%x\n\n", addr[2]);
	}
#endif
	//TODO: dma transfer

	sark_msg_free(sdp_msg);
}

#if(DEBUG_LEVEL==1)
void timerHandler(uint tick, uint null)
{
	myTick++;
	io_printf(IO_BUF, "Tick-%d\n", tick);
	timer_schedule_proc(timerHandler, myTick, 0, TIMER_TICK_PERIOD_US);	// Reschedule ourselves again
}
#endif

void c_main(void)
{
	myCoreID = sark_core_id();

	event_register_queue(dmaDoneHandler, EVENT_DMA, SLOT_0, PRIO_0);
	event_register_queue(sdpHandler, EVENT_SDP, SLOT_1, PRIO_1);

#if(DEBUG_LEVEL==1)	// only for debugging, should be removed at final version!!!
	event_register_timer(SLOT_2);
	event_queue_proc(timerHandler, 0, 0, PRIO_2);
#endif

	event_start(0, 0, SYNC_NOWAIT);
}



