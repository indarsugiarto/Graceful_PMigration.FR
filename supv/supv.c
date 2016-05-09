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

#define DEMO_HELLOW			1
#define DEMO_PINGPONG		2
#define DEMO				DEMO_HELLOW			// 1 = helloW, 2 = pingpong
#define STUB_TARGET_CORE	2	// for demo, we put pmagent in core-2
#define HELLOW_CORE			3	// and the hellowW in core-3 (see run.ybug)

/* forward declaration*/
void triggerDemo1();
void triggerDemo2();
void printReport(uint arg0, uint arg1);
/* test scenarios */
void test2(uint arg0, uint arg1);
void test3(uint arg0, uint arg1);

sdp_msg_t *msg;
uint myCoreID;
app_stub_t as[MAX_AVAIL_CORE];	// in reality, we have max 15 cores running in a chip, because 1 core is a spare

// forward declarations
void timeout(uint tick, uint null);

void initAppStub()
{
	uchar i;
    // then allocate SDRAM to hold TCM for pmagent
	uint szTCM = (APP_ITCM_SIZE+APP_DTCM_SIZE)*MAX_AVAIL_CORE; //ITCM+DTCM for all MAX_AVAIL_CORE cores
	uint *sdram_addr = (uint *)sark_xalloc(sv->sdram_heap, szTCM, SUPERVISOR_APP_ID, ALLOC_LOCK);
	if(sdram_addr==NULL) {
		io_printf(IO_STD, "[supv] Fatal Error: SDRAM allocation for TCM storage!\n");
		rt_error(RTE_ABORT);
	}
    else {
		io_printf(IO_BUF, "[supv] Allocating TCMSTD at 0x%x\n", sdram_addr);
        // then assign each TCM buffer accordingly
		// each TCM is composed of ITCM (0x5F00) and DTCM (0xE000)
		uint szITCM = PMAPP_ITCM_SIZE, szDTCM = PMAPP_DTCM_SIZE;
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
#if (DEMO==DEMO_HELLOW)
		triggerDemo1();
#elif (DEMO==DEMO_PINGPONG)
		triggerDemo2();
#endif
	}
	// Let's use:
	// TEST1_TRIGGERING_PORT for get the supv status
	// TEST2_TRIGGERING_PORT for ...
	// TEST3_TRIGGERING_PORT for ...
	else if(port==TEST1_TRIGGERING_PORT) {
		spin1_schedule_callback(printReport, 0, 0, PRIORITY_SDP);
	}
	else if(port==TEST2_TRIGGERING_PORT) {
		spin1_schedule_callback(test2, 0, 0, PRIORITY_SDP);
	}
	else
	spin1_msg_free(msg);
}

void hFR(uint key, uint payload)
{
	ushort sender, sType;
	sender = key >> 16;
	sType = key & 0xFFFF;
	// KEY_APP_ASK_TCMSTG, during initial run, the app will ask TCMSTG for future check-pointing
	if(sType==KEY_APP_ASK_TCMSTG) {
#if (DEMO==DEMO_HELLOW)
		// just for test with helloW
		if(sender != HELLOW_CORE) {
			io_printf(IO_STD, "[supv] HelloW core is different!\n");
			io_printf(IO_BUF, "[supv] HelloW core is different!\n");
		}
#endif
		uint newRoute = 1 << (sender+6);	// NOTE: core-1 is used for supv
		rtr_fr_set(newRoute);
		// first, send the ITCMSTG
		key = KEY_SUPV_REPLY_ITCMSTG;
		payload = (uint)as[sender-2].itcm_addr;
		spin1_send_fr_packet(key, payload, WITH_PAYLOAD);
		io_printf(IO_BUF, "[supv] ITCMSTG 0x%x was sent to core-%d\n", payload, sender);
		// second, send the DTCMSTG
		key = KEY_SUPV_REPLY_DTCMSTG;
		payload = (uint)as[sender-2].dtcm_addr;
		spin1_send_fr_packet(key, payload, WITH_PAYLOAD);
		io_printf(IO_BUF, "[supv] DTCMSTG 0x%x was sent to core-%d\n", payload, sender);
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
	io_printf(IO_STD, "[supv] Will do helloW assuming helloW is running...\n");

	// TODO: tell stub the location of itcm and dtcm
	uint newRoute = 1 << (STUB_TARGET_CORE+6);			// NOTE: core-1 is used for supv
	uint key = KEY_SUPV_TRIGGER_ITCMSTG;				// first, itcm
	uint payload = (uint)as[HELLOW_CORE-2].itcm_addr;
	rtr_fr_set(newRoute);								// send to STUB_TARGET_CORE
	key += (myCoreID << 16);							// add supv core info
	spin1_send_fr_packet(key, payload, WITH_PAYLOAD);	// send!
	io_printf(IO_BUF, "[supv] ITCMSTG 0x%x was sent to core-%d\n", payload, STUB_TARGET_CORE);
	key = KEY_SUPV_TRIGGER_DTCMSTG;						// next, dtcm
	key += (myCoreID << 16);
	payload = (uint)as[HELLOW_CORE-2].dtcm_addr;
	spin1_send_fr_packet(key, payload, WITH_PAYLOAD);	// send!
	io_printf(IO_BUF, "[supv] DTCMSTG 0x%x was sent to core-%d\n", payload, STUB_TARGET_CORE);
	rtr_fr_set(0);										// reset FR register
}

void triggerDemo2()
{

}

void printReport(uint arg0, uint arg1)
{
	// basic ID report
	io_printf(IO_STD, "[supv] supv core-ID = %d ( Please check if it is not 1! )\n", sark_core_id());
	io_printf(IO_STD, "[supv] MAX_AVAIL_CORE = %d\n", MAX_AVAIL_CORE);
	// report on sdram allocation
	for(uint i=0; i<MAX_AVAIL_CORE; i++) {
		io_printf(IO_STD, "\n[supv] Core-%d itcm_addr buffer = 0x%x\n", as[i].coreID, as[i].itcm_addr);
		io_printf(IO_STD, "[supv] Core-%d dtcm_addr buffer = 0x%x\n", as[i].coreID, as[i].dtcm_addr);
	}
	io_printf(IO_STD, "[supv] -------- End of report ---------\n\n");
}

/* Test scenarios */
/* test2:
 * - send TCMSTG to pmagent on core-2
 */
void test2(uint arg0, uint arg1)
{
	ushort pmagentCore  = 2;
	uint key, payload, newRoute = 1 << (pmagentCore+6);	// NOTE: core-1 is used for supv
	rtr_fr_set(newRoute);
	// first, send the ITCMSTG
	key = KEY_SUPV_TRIGGER_ITCMSTG;
	payload = (uint)as[pmagentCore-2].itcm_addr;
	spin1_send_fr_packet(key, payload, WITH_PAYLOAD);
	// second, send the DTCMSTG
	key = KEY_SUPV_TRIGGER_DTCMSTG;
	payload = (uint)as[pmagentCore-2].dtcm_addr;
	spin1_send_fr_packet(key, payload, WITH_PAYLOAD);
	rtr_fr_set(0);	// reset FR register
}

/* Let's use test3 for pinging a pmagent (if it is still alive)
 * */
void test3(uint arg0, uint arg1)
{

}
