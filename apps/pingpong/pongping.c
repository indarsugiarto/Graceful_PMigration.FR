/* SYNOPSIS
 *		This is the sdp version of pingpong.
 *		purpose: to see if sdp mechanism is worse or better than mcpl
 *		hasilnya: sdp version lebih besar dibandingkan mcpl version!!!
 */

//#define USE_SARK
/*---------------------------------------------------------------------*/
/* NOTE: Don't forget to add API=0 in the make process if you use SARK */
/*---------------------------------------------------------------------*/

#include "../pmigration.h"

#define USE_SARK_OR_API			USE_API

#if(USE_SARK_OR_API==USE_SARK)
#include <sark.h>
uint myTicks = 0;
#else
#include <spin1_api.h>
#endif


uint myCoreID;
uint dtcm_addr = 0;
uint itcm_addr = 0;
sdp_msg_t *myMsg;

void timerHandler(uint arg0, uint arg1)
{
#if(USE_SARK_OR_API==USE_SARK)
	myTicks++;
	//pkt_tx_kd(myCoreID, myTicks);
	timer_schedule_proc(timerHandler, 0, 0, TIMER_TICK_PERIOD_US);
#else
	myMsg->arg1 = arg0;
	spin1_send_sdp_msg(myMsg, DEF_SDP_TIMEOUT);
#endif
}

void sdpHandler(uint mailbox, uint port)
{
	sdp_msg_t *msg = (sdp_msg_t *) mailbox;

	// if the sender is the pinger
	uchar sender = msg->srce_port & 0x1F;
	if(sender == PINGER_CORE)
		io_printf(IO_BUF, "pong tick-%u\n", msg->arg1);
	else
		io_printf(IO_STD, "ping tick-%u\n", msg->arg1);
	spin1_msg_free(msg);
}

/* Use backupTCM to copy TCM to SDRAM and inform supervisor about the address
 *
 * */
void backupTCM()
{
	// since this backupTCM is called before loop event, then dmaDone won't effective
}

void c_main(void)
{
	uint rc;

#if(USE_SARK_OR_API==USE_SARK)
	myCoreID = sark_core_id();
#else
	myCoreID = spin1_get_core_id();
#endif

	if((myCoreID == PINGER_CORE) || (myCoreID == PONGER_CORE)) {
		// init sdp message
		myMsg->flags = 0x07;
		myMsg->tag = 0;
		myMsg->dest_addr = sv->p2p_addr;
		myMsg->srce_addr = sv->p2p_addr;
		myMsg->srce_port = (uchar)myCoreID | 0x20;
		if(myCoreID==PINGER_CORE)
			myMsg->dest_port = PONGER_CORE | 0x20;
		else
			myMsg->dest_port = PINGER_CORE | 0x20;
		myMsg->cmd_rc = STUB_TRIGGER_CMD;
		myMsg->seq = STUB_TRIGGER_SEQ;
		myMsg->length = sizeof(sdp_hdr_t) + sizeof(cmd_hdr_t);

#if(USE_SARK_OR_API==USE_SARK)
		io_printf(IO_BUF, "Registering event\n");
		// register packet buffer to be used by pkt_tx_kd()
		event_register_pkt(64, SLOT_0);
		event_register_timer(SLOT_1);
		// register event handler
		//event_register_queue(timerHandler, EVENT_TIMER, SLOT_1, PRIO_0);
		event_register_queue(mcplHandler, EVENT_RXPKT, SLOT_2, PRIO_1);
		// schedule the timer
		timer_schedule_proc(timerHandler, 0, 0, TIMER_TICK_PERIOD_US);
#else
		io_printf(IO_BUF, "Registering event\n");
		spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
		spin1_callback_on(TIMER_TICK, timerHandler, TIMER_PRIORITY);
		spin1_callback_on(SDP_PACKET_RX, sdpHandler, SDP_PRIORITY);
#endif

		// a notification
		if(myCoreID==PINGER_CORE)
			io_printf(IO_STD, "Pinger in core-%u is ready!\n", myCoreID);
		else
			io_printf(IO_STD, "Ponger in core-%u is ready!\n", myCoreID);

		// TODO: DMA transfer for itcm and dtcm
		backupTCM();

		// and, go!
#if(USE_SARK_OR_API==USE_SARK)
		rc = event_start(0, 0, SYNC_NOWAIT);
#else
		rc = spin1_start(SYNC_NOWAIT);
#endif

		// farewell
		io_printf(IO_STD, "Terminate with return code %d\n", rc);
	}
	else
	{
		io_printf(IO_STD, "I'm in core-%u but supposed to be in core-%u or core-%u\n", myCoreID, PINGER_CORE, PONGER_CORE);
	}
}

