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
uint dmaITCMid, dmaDTCMid;
uint readyCheckPointing;

//------------- for simple dma testing ---------------
#define DO_SIMPLE_DMA_TESTING	FALSE
uint dataBuffer[100];
uint *testBuffer;
uint testDMAid;
//----------------------------------------------------

// forward declaration
void testSimpleDMA(uint arg0, uint arg1);
void storeTCM(uint arg0, uint arg1);

// ======================================== Implementations ===========================================
void hTimer(uint tick, uint null)
{
	if(tick<5) {
		io_printf(IO_STD, "[helloW] %d-s before action!\n", 5-tick);
		//io_printf(IO_STD, "Hello from core-%d with appID-%d\n", sark_core_id(), sark_app_id());
		return;
	}
	if(tick==5) {
		io_printf(IO_STD, "[helloW] Requesting TCMSTG...\n");
		uint key, route;
		route = 1 << SUPV_CORE_IDX;
		key = (myCoreID << 16) + KEY_APP_ASK_TCMSTG;
		rtr_fr_set(route);
		spin1_send_fr_packet(key, 0, WITH_PAYLOAD);
		rtr_fr_set(0);
	}
	// otherwise, do checkpointing...
	else {
		if(readyCheckPointing==1) {
			io_printf(IO_STD, "[helloW] checkpointing...\n");
			spin1_schedule_callback(storeTCM, 0, 0, PRIORITY_DMA);
		}
		else {
			io_printf(IO_STD, "[helloW] Not ready for checkpointing...!\n");
		}
	}
}

// TODO: Sampai 6 Mei, 19:00 DMA belum bekerja dengan benar!

void hDMA(uint id, uint tag)
{
	if(tag==DMA_TRANSFER_ITCM_TAG)
		io_printf(IO_BUF, "[helloW] ITCM done!\n");
	else if(tag==DMA_TRANSFER_DTCM_TAG)
		io_printf(IO_BUF, "[helloW] DTCM done!\n");
	else
		io_printf(IO_BUF, "[helloW] dma id-%d, tag-0x%x\n",id,tag);
}

// storeTCM() will call dma for storing ITCM and DTCM
void storeTCM(uint arg0, uint arg1)
{
	io_printf(IO_BUF, "[helloW] Calling dma for ITCM...");
	dmaITCMid = spin1_dma_transfer(DMA_TRANSFER_ITCM_TAG, (void *)itcm_addr,
								   (void *)APP_ITCM_BASE, DMA_WRITE, APP_ITCM_SIZE);
    if(dmaITCMid!=0)
		io_printf(IO_BUF, "done!\n");
    else
		io_printf(IO_BUF, "fail!\n");
	io_printf(IO_BUF, "[helloW] Calling dma for DTCM...");
	dmaDTCMid = spin1_dma_transfer(DMA_TRANSFER_DTCM_TAG, (void *)dtcm_addr,
								   (void *)APP_DTCM_BASE, DMA_WRITE, APP_DTCM_SIZE);
    if(dmaDTCMid!=0)
		io_printf(IO_BUF, "done!\n");
    else
		io_printf(IO_BUF, "fail!\n");
}

void hFR(uint key, uint payload)
{
	uint sender, sType;
	sender = key >> 16;
	sType = key & 0xFFFF;
	if(sType==KEY_SUPV_REPLY_ITCMSTG) {
		itcm_addr = (uint *)payload;
		TCMSTGcntr++;
		io_printf(IO_STD, "[helloW] got ITCMSTG = 0x%x\n", itcm_addr);
	}
	else if(sType==KEY_SUPV_REPLY_DTCMSTG) {
		dtcm_addr  =(uint *)payload;
		TCMSTGcntr++;
		io_printf(IO_STD, "[helloW] got DTCMSTG = 0x%x\n", dtcm_addr);
	}
	if(TCMSTGcntr==2) {
#if(DO_SIMPLE_DMA_TESTING==TRUE)
		spin1_schedule_callback(testSimpleDMA, 0, 0, PRIORITY_DMA);
#else
		//spin1_schedule_callback(storeTCM, 0, 0, PRIORITY_DMA);
		readyCheckPointing = 1;
#endif
	}
}

void c_main (void)
{
	myCoreID = sark_core_id();
	readyCheckPointing = 0;	// 0 means not n0t ready, 1 means read1
	TCMSTGcntr = 0;	// 2 means both ITCMSTG and DTCMSTG are available

	io_printf(IO_STD, "[helloW] Starting of c_main at 0x%x with base itcm at 0x%x and base dtcm at 0x%x\n\n", c_main, APP_ITCM_BASE, APP_DTCM_BASE);

    //-------------- generate data test for dma and allocate sdram for testing -------------
#if(DO_SIMPLE_DMA_TESTING==TRUE)
    io_printf(IO_STD, "Allocating sdram...");
    sark_srand ((sark_chip_id () << 8) + sark_core_id() * sv->time_ms);
    for(ushort i=0; i<100; i++)
        dataBuffer[i] = sark_rand();
    testBuffer = sark_xalloc(sv->sdram_heap, 100*sizeof(uint), sark_app_id(), ALLOC_LOCK);
    if(testBuffer==NULL) {
        io_printf(IO_STD, "xalloc error!\n");
        rt_error(RTE_ABORT);
    }
    else
        io_printf(IO_STD, "done!\n");
#endif
    //--------------------------------------------------------------------------------------

    spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
    spin1_callback_on(TIMER_TICK, hTimer, TIMER_PRIORITY);
    spin1_callback_on(FRPL_PACKET_RECEIVED, hFR, PRIORITY_FR);
    spin1_callback_on(DMA_TRANSFER_DONE, hDMA, PRIORITY_DMA);
    spin1_start(SYNC_NOWAIT);
}






/*======================================== Misc test procedures ===========================================*/
// testSimpleDMA() result: OK
void testSimpleDMA(uint arg0, uint arg1)
{
	io_printf(IO_STD, "[helloW] Calling dma for test...");
	testDMAid = spin1_dma_transfer(0xc0bac0ba, (void *)testBuffer, (void *)dataBuffer,
								   DMA_WRITE, 100*sizeof(uint));
	if(testDMAid!=0)
		io_printf(IO_STD, "done!\n");
	else
		io_printf(IO_STD, "fail!\n");
}

