#include <spin1_api.h>
#include "../../include/pmigration.h"

#define TIMER_TICK_PERIOD_US	1000000	// in milliseconds
#define TIMER_PRIORITY		0

#define SUPV_CORE_IDX		7	// which is equivalent to core-1

uint myAppID;
uint myCoreID;

uint *itcm_addr;
uint *dtcm_addr;

uchar TCMSTGcntr;

void hTimer(uint tick, uint null)
{
	if(tick<5) {
		io_printf(IO_STD, "Hello from core-%d with appID-%d\n", sark_core_id(), sark_app_id());
		return;
	}
	if(tick==5) {
		io_printf(IO_STD, "Requesting TCMSTG...\n");
		uint key, route;
		route = 1 << SUPV_CORE_IDX;
		key = (myCoreID << 16) + KEY_APP_ASK_TCMSTG;
		rtr_fr_set(route);
		spin1_send_fr_packet(key, 0, WITH_PAYLOAD);
		rtr_fr_set(0);
	}
}

// storeTCM() will call dma for storing ITCM and DTCM
void storeTCM(uint arg0, uint arg1)
{

}

void hFR(uint key, uint payload)
{
	uint sender, sType;
	sender = key >> 16;
	sType = key & 0xFFFF;
	if(sType==KEY_SUPV_REPLY_ITCMSTG) {
		itcm_addr = (uint *)payload;
		TCMSTGcntr++;
		io_printf(IO_STD, "ITCMSTG = 0x%x\n", itcm_addr);
	}
	else if(sType==KEY_SUPV_REPLY_DTCMSTG) {
		dtcm_addr  =(uint *)payload;
		TCMSTGcntr++;
		io_printf(IO_STD, "DTCMSTG = 0x%x\n", dtcm_addr);
	}
	if(TCMSTGcntr==2)
		spin1_schedule_callback(storeTCM, 0, 0, PRIORITY_DMA);
}

void c_main (void)
{
	myCoreID = sark_core_id();
	TCMSTGcntr = 0;	// 2 means both ITCMSTG and DTCMSTG are available

	io_printf(IO_STD, "Starting of c_main at 0x%x\n\n", c_main); sark_delay_us(1000);

    spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
    spin1_callback_on(TIMER_TICK, hTimer, TIMER_PRIORITY);
	spin1_callback_on(FRPL_PACKET_RECEIVED, hFR, PRIORITY_FR);
    spin1_start(SYNC_NOWAIT);
}
