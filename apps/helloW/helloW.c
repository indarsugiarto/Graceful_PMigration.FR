#include <spin1_api.h>
#include "../../include/pmigration.h"

#define TIMER_TICK_PERIOD_US	1000000	// in milliseconds
#define TIMER_PRIORITY		0

#define SUPV_CORE_IDX		7	// which is equivalent to core-1

uint myAppID;
uint myCoreID;

uint *itcm_addr;
uint *dtcm_addr;
uint ajmp;

uint myTick;

uchar TCMSTGcntr;
uint dmaITCMid, dmaDTCMid;
volatile uint readyCheckPointing;

//------------- for simple dma testing ---------------
#define DO_SIMPLE_DMA_TESTING	FALSE
uint dataBuffer[100];
uint *testBuffer;
uint testDMAid;
//----------------------------------------------------

// forward declaration
void c_main();
void testSimpleDMA(uint arg0, uint arg1);
void storeTCM(uint arg0, uint arg1);


// ============================ Implementations ====================================

// use mrs to move the contents of a PSR to a general-purpose register.
#ifdef THUMB
extern uint getCPSR();
#else

__inline uint getCPSR()
{
  uint _cpsr;

  asm volatile (
	"mrs	%[_cpsr], cpsr \n"
	 : [_cpsr] "=r" (_cpsr)
	 :
	 : );

  return _cpsr;
}
#endif

void hTimer(uint tick, uint null)
{
	//uint core = spin1_get_core_id();
	uint core = sark_core_id();
	myTick++;
	io_printf(IO_STD, "[helloW-from-c%d] myTick = %d\n", core, myTick);
	if(tick<5) {
		io_printf(IO_STD, "[helloW-from-c%d] %d-s before action!\n", core,5-tick);
	}
	else if(tick==5) {
		io_printf(IO_STD, "[helloW-from-c%d] Requesting TCMSTG...\n", core);
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
			io_printf(IO_BUF, "[helloW-from-c%d] checkpointing %d...\n", core, tick-5);
			spin1_schedule_callback(storeTCM, 0, 0, PRIORITY_DMA);
		}
		else {
			io_printf(IO_STD, "[helloW-from-c%d] Not ready for checkpointing...!\n", core);
		}
	}
}

// TODO: Sampai 6 Mei, 19:00 DMA belum bekerja dengan benar!

void hDMA(uint id, uint tag)
{
	return;
	// uint core = spin1_get_core_id();
	uint core = sark_core_id();
	if(tag==DMA_TRANSFER_ITCM_TAG)
		io_printf(IO_BUF, "[helloW-from-c%d] ITCM done!\n", core);
	else if(tag==DMA_TRANSFER_DTCM_TAG)
		io_printf(IO_BUF, "[helloW-from-c%d] DTCM done!\n", core);
	else
		io_printf(IO_BUF, "[helloW-from-c%d] dma id-%d, tag-0x%x\n", core, id,tag);
}

// storeTCM() will call dma for storing ITCM and DTCM
void storeTCM(uint arg0, uint arg1)
{
	//uint core = spin1_get_core_id();
	uint core = sark_core_id();
	io_printf(IO_BUF, "[helloW-from-c%d] Calling dma for ITCM...", core);
	dmaITCMid = spin1_dma_transfer(DMA_TRANSFER_ITCM_TAG, (void *)itcm_addr,
								   (void *)APP_ITCM_BASE, DMA_WRITE, APP_ITCM_SIZE);
	if(dmaITCMid!=0)
		io_printf(IO_BUF, "done!\n");
    else
		io_printf(IO_BUF, "fail!\n");
	io_printf(IO_BUF, "[helloW-from-c%d] Calling dma for DTCM...", core);
	dmaDTCMid = spin1_dma_transfer(DMA_TRANSFER_DTCM_TAG, (void *)dtcm_addr,
								   (void *)APP_DTCM_BASE, DMA_WRITE, APP_DTCM_SIZE);
	if(dmaDTCMid!=0)
		io_printf(IO_BUF, "done!\n");
    else
		io_printf(IO_BUF, "fail!\n");

	// send AJMP to supv
	uint key, route, payload;
	route = 1 << SUPV_CORE_IDX;
	rtr_fr_set(route);
	/*
	// send registers
	uint regs[32];
	asm volatile (" ldr r1, =ajmp_addr \n\
					ldr r0, [r1] \n\
					mov pc, r0"
				 );
	for(uint i=0; i<32; i++) {
		key = (myCoreID << 16) + KEY_APP_SEND_Rx;
	}
	*/

	key = (myCoreID << 16) + KEY_APP_SEND_AJMP;

	/* Several scenarios:
	 * ajmp <- c_main
	 * ajmp <- storeTCM (this routine)
	 * ajmp <- jmp_label (right at the point)
	 * ajmp <- pc (in assembly)
	*/
	//ajmp = (uint *)storeTCM;	// doesn't work
	//ajmp = (uint)c_main;		// OK-ish, but produce "Not ready for checkpointing..."
	ajmp = (uint)spin1_start;	// OK, but with misleading identity :)
	//ajmp = (uint)&&jmp_label;

	/*
	// copy pc into ajmp
	asm volatile (" ldr r7, =ajmp \n\
					mov r6, pc \n\
					str r6, [r7]"
				 );
	*/
// jmp_label:
	payload = ajmp;
	spin1_send_fr_packet(key, payload, WITH_PAYLOAD);
	/*
	io_printf(IO_BUF, "[helloW-from-c%d] sending AJMP 0x%x, "
					  "where storeTCM is at 0x%x, jmp_label is at 0x%x "
					  "and c_main is at 0x%x\n", core, ajmp, storeTCM, &&jmp_label, c_main);
	*/
	// and stack pointer or cspr?
	// key = (myCoreID << 16) + KEY_APP_SEND_SPTR;
	key = (myCoreID << 16) + KEY_APP_SEND_CPSR;
	//payload = getCPSR();
	//spin1_send_fr_packet(key, payload, WITH_PAYLOAD);


	// finally, reset FR register
	rtr_fr_set(0);
jmp_label:
	asm volatile ("mov r0, r0");
}

void hFR(uint key, uint payload)
{
	uint sender, sType;
	//uint core = spin1_get_core_id();
	uint core = sark_core_id();
	sender = key >> 16;
	sType = key & 0xFFFF;
	if(sType==KEY_SUPV_REPLY_ITCMSTG) {
		itcm_addr = (uint *)payload;
		TCMSTGcntr++;
		io_printf(IO_STD, "[helloW-from-c%d] got ITCMSTG = 0x%x\n", core, itcm_addr);
	}
	else if(sType==KEY_SUPV_REPLY_DTCMSTG) {
		dtcm_addr  =(uint *)payload;
		TCMSTGcntr++;
		io_printf(IO_STD, "[helloW-from-c%d] got DTCMSTG = 0x%x\n", core, dtcm_addr);
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

	io_printf(IO_STD, "[helloW-from-c%d] Starting of c_main at 0x%x with base itcm at 0x%x "
					  "and base dtcm at 0x%x\n\n", myCoreID, c_main, APP_ITCM_BASE,
			  APP_DTCM_BASE);

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






/*============================ Misc test procedures ===========================*/
// testSimpleDMA() result: OK
void testSimpleDMA(uint arg0, uint arg1)
{
	//uint core = spin1_get_core_id();
	uint core = sark_core_id();
	io_printf(IO_STD, "[helloW-from-c%d] Calling dma for test...", core);
	testDMAid = spin1_dma_transfer(0xc0bac0ba, (void *)testBuffer, (void *)dataBuffer,
								   DMA_WRITE, 100*sizeof(uint));
	if(testDMAid!=0)
		io_printf(IO_STD, "done!\n");
	else
		io_printf(IO_STD, "fail!\n");
}

