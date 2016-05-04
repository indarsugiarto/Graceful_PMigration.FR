/* SYNOPSIS
 *		Stub will stay somewhere at higher ITCM and response to a call via FR to transfer
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
 * 
 * 12.03.2015
 * - Use FR instead since MCPL and SDP will be used by other apps
 * */

/* =================================================================================================================
 * TODO: Copy spinnaker_tools_134 ke folder Gracefule_PMigration.FR karena harus memodifikasi spinnaker.h dan sark.h
   lalu meng-compile ulang supaya bisa dipakai di sini!!!!
   =================================================================================================================*/

#include <spin1_api.h>
#include "../include/pmigration.h"

uint stubTriggerKey;
uint *dtcm_addr = 0;
uint *itcm_addr = 0;
uint ajmp_addr = 0;
uint dmaCntr = 0;

#define FETCH_ITCM	0xfe7c41
#define FETCH_DTCM	0xfe7c4d

void hDMADone(uint tid, uint tag)
{
	// jump to app-addr
	dmaCntr++;
	if(dmaCntr==2) { // both ITCM and DTCM are fetched
		// jump to AJMP
	}
}

// getTCM will be scheduled whenever *tcm_addr != 0
void getTCM(uint arg0, uint arg1)
{
	uint status;
	// get itcm
	status = spin1_dma_transfer(FETCH_ITCM, (void *)itcm_addr, (void *)APP_PROG_BASE, DMA_READ, APP_PROG_SIZE);	 // direction: 0 = transfer to TCM, 1 = transfer to system
#if(DEBUG_LEVEL==1)
	if(status==FAILURE)
		io_printf(IO_STD, "DMA-ITCM fail!\n");
#endif
	// get dtcm
	status  = spin1_dma_transfer(FETCH_DTCM, (void *)dtcm_addr, (void *)APP_DTCM_BASE, DMA_READ, APP_DATA_SIZE);
#if(DEBUG_LEVEL==1)
	if(status==FAILURE)
		io_printf(IO_STD, "DMA-DTCM fail!\n");
#endif
}

void hFR(uint key, uint payload)
{
	ushort sender, sType;
	sender = key >> 16;
	sType = key & 0xFFFF;
	if(key==KEY_SUPV_TRIGGER_ITCMSTG) {
		itcm_addr = (uint *)payload;
		if(itcm_addr != 0 && dtcm_addr != 0) spin1_schedule_callback(getTCM, 0, 0, PRIORITY_DMA);
	}
	else if(key==KEY_SUPV_TRIGGER_DTCMSTG) {
		dtcm_addr = (uint *)payload;
		if(itcm_addr != 0 && dtcm_addr != 0) spin1_schedule_callback(getTCM, 0, 0, PRIORITY_DMA);
	}
}

#if(DEBUG_LEVEL==1)
void hTimer(uint tick, uint null)
{
	io_printf(IO_BUF, "Tick-%d\n", tick);
}
#endif

void c_main(void)
{
#if(DEBUG_LEVEL==1) // for debugging only. In release version, it will be removed!
	io_printf(IO_STD, "pmagent @ core-%d id-%d\n",sark_core_id(), sark_app_id());
	spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
	spin1_callback_on(TIMER_TICK, hTimer, PRIORITY_TIMER);
#endif

	spin1_callback_on(FRPL_PACKET_RECEIVED, hFR, PRIORITY_FR);
	spin1_callback_on(DMA_TRANSFER_DONE, hDMADone, PRIORITY_DMA);
	spin1_start(SYNC_NOWAIT);
}



