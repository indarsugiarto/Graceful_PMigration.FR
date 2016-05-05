#include <spin1_api.h>
#include "../include/pmigration.h"

/* Percobaan Wed, 4 Mei, 17:54
1. Fungsi io_printf() berjalan baik dengan IO_BUF, tapi tidak keluar
   kalau pakai IO_STD. Kenapa ya?
2. Jika itcm_addr, dtcm_addr, dmaCntr di inisialisasi diluar c_main,
   hTimer tidak dieksekusi. Kenapa ya?
3. Waktu pertama kali pmagent.aplx di load, spinnaker berhenti di INIT state
   lalu setelah pmagent.aplx di load sekali lagi, baru bisa berjalan normal.
   Kenapa ya?
*/

uint *itcm_addr;
uint *dtcm_addr;

uint ajmp_addr;
uint dmaCntr;

#define FETCH_ITCM      0xfe7c41
#define FETCH_DTCM      0xfe7c4d

void hDMADone(uint tid, uint tag)
{
        // jump to app-addr
        dmaCntr++;
        if(dmaCntr==2) { // both ITCM and DTCM are fetched
                // jump to AJMP
        }
}

void getTCM(uint arg1, uint arg2)
{
    io_printf(IO_BUF, "getTCM()\n");
}

void hTimer(uint tick, uint null)
{
	io_printf(IO_BUF, "Tick-%d\n", tick);
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

void c_main(void)
{
	itcm_addr = 0;
	dtcm_addr = 0;
	dmaCntr = 0;
	ajmp_addr = 0;

	io_printf(IO_BUF, "pmagent @ core-%d id-%d\n",sark_core_id(), sark_app_id());
	spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
	spin1_callback_on(TIMER_TICK, hTimer, PRIORITY_TIMER);
	spin1_callback_on(FRPL_PACKET_RECEIVED, hFR, PRIORITY_FR);
	spin1_callback_on(DMA_TRANSFER_DONE, hDMADone, PRIORITY_DMA);
	spin1_start(SYNC_NOWAIT);
}
