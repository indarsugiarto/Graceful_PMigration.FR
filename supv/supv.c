/* Flows:
 * 1. supv will be loaded first during which:
 *    1.1. Prepare SDRAM
 * 2. wait for request from pmagent
 *    2.1. pmagent will request the allocated SDRAM
 * 3. pmagent regularly do check-pointing
 *    3.1. pmagent might require modification of API: timer, dma
 *
 * When the pmagent send FR, it uses the following format:
 * key [core-id, SIGNAL_TYPE]
 * both core-id and SIGNAL_TYPE are ushort
 *
 * And the reply from supv will be like:
 * key [additional_info, SIGNAL_TYPE]
 * both additional_info and SIGNAL_TYPE are ushort.
 *
 * We assume that supv will be loaded into core-1
 * We allocate SDRAM for all MAX_AVAIL_CORE since it is pretty small: (32+64)*16=1536KB=1.5MB out of 128MB
 * */

/*
From pmigration.h:
---------------------
typedef struct app_stub		// application holder
{
    ushort coreID;
    ushort appID;
	//uint *tcm_addr;
	uint *itcm_addr;
	uint *dtcm_addr;
    uint *restart_addr;
	// uint registers[];	// should hold all registers of the microprocessors?
} app_stub_t;

*/

#include <spin1_api.h>
#include "../include/pmigration.h"	

#define DEMO	1			// 1 = helloW, 2 = pingpong

/* forward declaration*/
void triggerDemo1();
void triggerDemo2();

sdp_msg_t *msg;
uint myCoreID;
app_stub_t as[MAX_AVAIL_CORE];	// in reality, we have max 15 cores running in a chip, because 1 core is a spare

void timeout(uint tick, uint null);

void initAppStub()
{
	uchar i;
    // then allocate SDRAM to hold TCM for pmagent
	uint szTCM = (APP_PROG_SIZE+APP_DATA_SIZE)*MAX_AVAIL_CORE; //ITCM+DTCM for all MAX_AVAIL_CORE cores
	uint *sdram_addr = (uint *)sark_xalloc(sv->sdram_heap, szTCM, SUPERVISOR_APP_ID, ALLOC_LOCK);
	if(sdram_addr==NULL) {
		io_printf(IO_STD, "Fatal Error: SDRAM allocation for TCM storage!\n");
		rt_error(RTE_ABORT);
	}
    else {
        io_printf(IO_BUF, "Allocating TCMSTD at 0x%x\n", sdram_addr);
        // then assign each TCM buffer accordingly
		// each TCM is composed of ITCM (32*1024) and DTCM (64*1024)
		uint szITCM = 32*1024, szDTCM = 64*1024;
		uint *ptr = sdram_addr;
        for(i=0; i<MAX_AVAIL_CORE; i++) {
            as[i].coreID = i+2;	// since we assume supv is in core-1
			as[i].itcm_addr = ptr;
			as[i].dtcm_addr = ptr + szITCM;
			ptr += (szITCM + szDTCM);
        }
    }
}

void hSDP(uint mBox, uint port)
{
	sdp_msg_t *msg = (sdp_msg_t *)mBox;
	if(port==DEMO_TRIGGERING_PORT){
#if (DEMO==1)
		triggerDemo1();
#elif (DEMO==2)
		triggerDemo2();
#endif
	}
	spin1_msg_free(msg);
}

void hFR(uint key, uint payload)
{
	ushort sender, sType;
	sender = key >> 16;
	sType = key & 0xFFFF;
	// KEY_APP_ASK_TCMSTG, during initial run, the app will ask TCMSTG for future check-pointing
	if(sType==KEY_APP_ASK_TCMSTG) {
		uint newRoute = 1 << (sender+6);	// NOTE: core-1 is used for supv
		rtr_fr_set(newRoute);
		// first, send the ITCMSTG
		key = KEY_SUPV_REPLY_ITCMSTG;
		payload = (uint)as[sender-2].itcm_addr;
		spin1_send_fr_packet(key, payload, WITH_PAYLOAD);
		// second, send the DTCMSTG
		key = KEY_SUPV_REPLY_DTCMSTG;
		payload = (uint)as[sender-2].dtcm_addr;
		spin1_send_fr_packet(key, payload, WITH_PAYLOAD);
		rtr_fr_set(0);	// reset FR register
		return;
	}
	if(sType==KEY_APP_SEND_AJMP) {
		as[sender-2].restart_addr = (uint *)payload;
	}
}

void c_main(void)
{
	myCoreID = sark_core_id();

	// init appstub ("as") to hold all stubs' data
	initAppStub();

	spin1_callback_on(FRPL_PACKET_RECEIVED, hFR, PRIORITY_FR);

	// for demonstration, we use sdp to trigger migration demo
    spin1_callback_on(SDP_PACKET_RX, hSDP, PRIORITY_SDP);

    spin1_start(SYNC_NOWAIT);
}

/* For Demonstration purpose */
void triggerDemo1()
{

}

void triggerDemo2()
{

}
