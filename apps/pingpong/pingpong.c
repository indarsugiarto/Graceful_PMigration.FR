/* SYNOPSIS
 *		ping-core will send a "ping" to the pong-core every xxx-milliseconds
 *		purpose: to see if process migration can also handle routing table
 *
 * TODO: in the future, use Timer-2 to trigger backuping
 *
 * FLow:
 * 1. Init myself (get coreID, appID)
 * 2. Init router (because I want to use MCPL)
 * 3. Set timer and its callback
 * 4. Set MCPL callback
 * 5. Here is the first crucial part: send a request for TCM storage in SDRAM to supervisor
 * 6. Enter the event loop
 *
 * Rev:
 * 20.11.2015
 * - Remove initialization of router. Use router from supv-core instead!
 *
 * 14.12.2015
 * - Modifying requestTCMSTG() and backupTCM() to be more API-stylish! e.g. using two parameters: coreID and appID
 */

//#define USE_SARK
/*---------------------------------------------------------------------*/
/* NOTE: Don't forget to add API=0 in the make process if you use SARK */
/*---------------------------------------------------------------------*/

#include "../../include/pmigration.h"

#define USE_SARK_OR_API			USE_API

#if(USE_SARK_OR_API==USE_SARK)
#include <sark.h>
#else
#include <spin1_api.h>
#endif


uint myCoreID;
uint myAppID;
uint destCore;			// if I'm pinger, then destCore will be ponger, and vice versa
uint myTicks = 0;		// only usefull if I use sark

/* TODO: the following variables will be handled by API in the future */
uint dtcm_addr = 0;
uint itcm_addr = 0;
uint myAjmp = 0;

/*--------------------------------------------------------------------*/

/* Since BLX return address is located in stack, and stack is located in DTCM
 * that is backed-up, then we can compute AJMP from within backupTCM()
 */
void spin1_backupTCM(uint _myCoreID, uint _myAppID)
{
	io_printf(IO_BUF, "Core-%d start backuping TCM...\n", _myCoreID);
	// no storage available yet? get out! supv-core is supposed to tell where is my TCMSTG
	if(dtcm_addr==0 || itcm_addr==0) return;

	//TODO: something not right with the assembler. For now, let's skip it

	/* then fetch lr before doing dma transfer (or jumping to elsewhere) using example:
	LDR  R1, =globvar   ; read address of globvar into R1
	LDR  R0, [R1]       ; load value of globvar
	ADD  R0, R0, #2
	STR  R0, [R1]       ; store new value into globvar
	*/
	__asm volatile ("push {r0}\n"
					"push {r1}\n"
					"ldr r1, =myAjmp\n"
					"mov r0, lr\n"
					"str r0, [r1]\n"
					"pop {r1}\n"
					"pop {r0}");

	// TODO: should we copy both TCM or just DTCM?
	spin1_dma_transfer(_myCoreID, (void *)dtcm_addr, (void *)APP_DTCM_BASE, DMA_WRITE, APP_DTCM_SIZE);
	spin1_dma_transfer(!_myCoreID, (void *)itcm_addr, (void *)APP_ITCM_BASE, DMA_WRITE, APP_ITCM_SIZE);

	spin1_send_mc_packet(KEY_APP_SEND_AJMP | _myCoreID << 16 | _myAppID << 8, myAjmp, 1);
	//for debugging:
	io_printf(IO_BUF, "Checkpointing is done!\n");
}


// the following router initialization just initialize my self and doesn't do anything with process migration
void initRouter()
{
	if(myCoreID == PINGER_CORE) {
		uint e = rtr_alloc(2);
		if(e==0) {
			io_printf(IO_STD, "Fail to allocate routing table");
			rt_error(RTE_ABORT);
		}
		else {
			uint PINGER_ROUTE = (1 << (6+PONGER_CORE));
			uint PONGER_ROUTE = ( 1 << (6+PINGER_CORE));
			rtr_mc_set(e, PINGER_CORE, 0xFFFFFFFF, PINGER_ROUTE);
			rtr_mc_set(e+1, PONGER_CORE, 0xFFFFFFFF, PONGER_ROUTE);
		}
	}
}


/* Use timer for play ping pong, nothing todo with process migration */
void timerHandler(uint arg0, uint arg1)
{
	myTicks++;
#if(USE_SARK_OR_API==USE_SARK)
	pkt_tx_kd(myCoreID, myTicks);
	timer_schedule_proc(timerHandler, 0, 0, TIMER_TICK_PERIOD_US);
#else
	// TODO: the destCore SHOULD be altered after get notification from supv-core
	arg1 = destCore << 24;				// use arg1 yang nganggur daripada bikin variable baru
	arg1 |= myCoreID << 16;				// send to pinger core from ponger core, or, from to ponger from pinger

	// let's get back with the assumption that the app doesn't know anything about process migration
	arg1 = myCoreID;
	// io_printf(IO_STD, "Sending key 0x%x with payload %u\n", arg1, arg0); sark_delay_us(1000*myCoreID);
	spin1_send_mc_packet(arg1, arg0, 1);
#endif

	// before leaving this event handler, make a backup of TCM and AJMP
	spin1_backupTCM(myCoreID, myAppID);
}

void mcplHandler(uint key, uint payload)
{
	// part-1: special for pmigration only
	// TODO: move to API in the future!
	uint sender = (key & 0x00FF0000) >> 16;
	if(sender==17) {
		// if supv-core send the storage address for TCM
		if((key & 0x00FFFFFF) == KEY_SUPV_SEND_TCMSTG) {
			// for debugging:
			io_printf(IO_BUF, "Supv-core allocates TCMSTG at 0x%x\n", payload);
			itcm_addr = payload;
			dtcm_addr = payload + APP_ITCM_SIZE;
			spin1_backupTCM(myCoreID, myAppID);
		}
		// if supv-core send a trigger to make *me* sleep, assuming supv-core has allocated a replacing stub
		else if((key & 0x00FFFFFF) == KEY_SUPV_SEND_STOP) {
			spin1_exit(0);
		}
		return;
	}

	// part-2: the following should be nothing to do with the pmigration scenario,
	// it should be app independent, ie., should assume that app doesn't know anything about process migration
	if(key == PINGER_CORE)
		io_printf(IO_BUF, "pong tick-%u\n", payload);
	else if(key == PONGER_CORE)
		io_printf(IO_STD, "ping tick-%u\n", payload);
	else {	// must be sent by supv-core
	}
}

/* TODO:
 * requestTCMSTG() should be executed by a call as an API routine
 * */
void spin1_requestTCMSTG(uint _myCoreID, uint _myAppID)
{
	uint key = KEY_APP_ASK_TCMSTG | _myCoreID << 16 | _myAppID << 8;
	// below is just for debugging...
	io_printf(IO_STD, "Sending KEY_APP_ASK_TCMSTG (0x%x) to supv-core...\n", key); sark_delay_us(_myCoreID*1000);
	// send notification to supervisor (key format: "ccssiiyy") to ask where is the storage for ITCM
	spin1_send_mc_packet(key, 0, 1);
}

void c_main(void)
{
	//myAjmp = (uint)c_main;
	uint rc;

#if(USE_SARK_OR_API==USE_SARK)
	myCoreID = sark_core_id();
#else
	myCoreID = spin1_get_core_id();
	myAppID = sark_app_id();
#endif

	if((myCoreID == PINGER_CORE) || (myCoreID == PONGER_CORE)) {

		destCore = (myCoreID==PINGER_CORE?PONGER_CORE:PINGER_CORE);
		initRouter();
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
		io_printf(IO_STD, "Registering event: timer and mcpl\n"); sark_delay_us(myCoreID*1000);
		spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
		spin1_callback_on(TIMER_TICK, timerHandler, TIMER_PRIORITY);
		spin1_callback_on(MCPL_PACKET_RECEIVED, mcplHandler, MCPL_PRIORITY);
#endif

		// a notification
		if(myCoreID==PINGER_CORE)
			io_printf(IO_STD, "Pinger in core-%u is ready!\n", myCoreID);
		else
			io_printf(IO_STD, "Ponger in core-%u is ready!\n", myCoreID);

		// TODO: DMA transfer for itcm and dtcm
		// backupTCM(); -> must be called within event loop	
		// spin1_schedule_callback(requestTCMSTG, 0, 0, BACKUP_PRIORITY); -> doesn't work prior to event loop
		spin1_requestTCMSTG(myCoreID, myAppID);

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


/******************************************************************************************************************
 ************************************************** KUBURAN *******************************************************
 * ----------------------------------------------------------------------------------------------------------------

******************************************************************************************************************/

