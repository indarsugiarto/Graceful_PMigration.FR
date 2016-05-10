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

   Percobaan Tue, 10 Mei, 11:50
   Kalau pakai io_printf() dengan IO_BUF, pmagent OK dan tidak perlu
   re-load dua kali. Kalau pake IO_STD, macet di state "SARK" atau semacamnya.
*/

uint *itcm_addr;
uint *dtcm_addr;

uint ajmp_addr;
uint dmaCntr;

uint dmaDTCMid;
uint dmaITCMid;

#define FETCH_ITCM      0xfe7c41
#define FETCH_DTCM      0xfe7c4d

void hDMADone(uint tid, uint tag)
{
	io_printf(IO_BUF, "dma id-0x%x, tag-0x%x\n", tid, tag);
	dmaCntr++;
	if(dmaCntr==2 && ajmp_addr !=0) { // both ITCM and DTCM are fetched
		io_printf(IO_BUF, "[pmagent] dma complete!\n[pmagent] TODO: align sptr and jump to AJMP at 0x%x\n", ajmp_addr);
		// jump to AJMP

		/* then fetch lr before doing dma transfer (or jumping to elsewhere) using example:
		LDR  R1, =globvar   ; read address of globvar into R1
		LDR  R0, [R1]       ; load value of globvar
		ADD  R0, R0, #2
		STR  R0, [R1]       ; store new value into globvar
		*/

		/*
		asm volatile ("push {r0}\n\t"
						"push {r1}\n\t"
						"ldr r1, =ajmp_addr\n\t"
						"mov r0, lr\n\t"
						"str r0, [r1]\n\t"
						"pop {r1}\n\t"
						"pop {r0}\n\t"
						"mov pc,r1"
					  );
		*/
		/*
		asm volatile (" ldr r1, =ajmp_addr \n\
						ldr r0, [r1] \n\
						mov pc, r0"
					 );
		*/
	}
}

void getTCM(uint arg1, uint arg2)
{
	//io_printf(IO_BUF, "[pmagent] getting*TCM()\n");
	uint szMem;
	//szMem = 32*1024; --> OK!
	szMem = APP_ITCM_SIZE;
	io_printf(IO_BUF, "[pmagent] getting %d-bytes DTCM...", szMem);
	//dmaDTCMid = spin1_dma_transfer(DMA_TRANSFER_DTCM_TAG, (void *)APP_DTCM_BASE, (void *)dtcm_addr, DMA_READ, APP_DTCM_SIZE);
	//dmaDTCMid = spin1_dma_transfer(DMA_TRANSFER_DTCM_TAG, (void *)APP_DTCM_BASE, (void *)dtcm_addr, DMA_READ, sizeof(uint));	--> OK

	dmaDTCMid = spin1_dma_transfer(DMA_TRANSFER_DTCM_TAG, (void *)APP_DTCM_BASE, (void *)dtcm_addr, DMA_READ, szMem);
	if(dmaDTCMid!=0)
		io_printf(IO_BUF, "done!\n");
	else
		io_printf(IO_BUF, "fail!\n");
	io_printf(IO_BUF, "[pmagent] getting %d-bytes ITCM ", APP_ITCM_SIZE);
	dmaITCMid = spin1_dma_transfer(DMA_TRANSFER_ITCM_TAG, (void *)APP_ITCM_BASE, (void *)dtcm_addr, DMA_READ, APP_ITCM_SIZE); // OK!
	if(dmaITCMid!=0)
		io_printf(IO_BUF, "done!\n");
	else
		io_printf(IO_BUF, "fail!\n");
}

void hTimer(uint tick, uint null)
{
	io_printf(IO_BUF, "[pmagent] Tick-%d\n", tick);
}

void hFR(uint key, uint payload)
{
	//io_printf(IO_BUF, "got key-0x%x (signal==0x%x) payload-0x%x\n", key, (key & 0xFFFF), payload);

	ushort sender, sType;
	sender = key >> 16;
	sType = key & 0xFFFF;
	if(sType==KEY_SUPV_TRIGGER_ITCMSTG) {
		itcm_addr = (uint *)payload;
		io_printf(IO_BUF, "[pmagent] got ITCMSTG @ 0x%x\n", itcm_addr);
		if(itcm_addr != 0 && dtcm_addr != 0) spin1_schedule_callback(getTCM, 0, 0, PRIORITY_DMA);
	}
	else if(sType==KEY_SUPV_TRIGGER_DTCMSTG) {
		dtcm_addr = (uint *)payload;
		io_printf(IO_BUF, "[pmagent] got DTCMSTG @ 0x%x\n", dtcm_addr);
		if(itcm_addr != 0 && dtcm_addr != 0) spin1_schedule_callback(getTCM, 0, 0, PRIORITY_DMA);
	}
	else if(sType==KEY_SUPV_TRIGGER_AJMP) {
		ajmp_addr = payload;
		//io_printf(IO_BUF, "[pmagent] got AJMP @ 0x%x\n", ajmp_addr);
	}
}

void c_main(void)
{
	// TODO: explain why variables should be "uninitialized"?
	itcm_addr = 0;
	dtcm_addr = 0;
	dmaCntr = 0;
	ajmp_addr = 0;

	dmaDTCMid = 0;
	dmaITCMid = 0;

	io_printf(IO_BUF, "[pmagent] running @ core-%d id-%d starting at 0x%x\n",sark_core_id(), sark_app_id(), c_main);
	spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
	spin1_callback_on(TIMER_TICK, hTimer, PRIORITY_TIMER);
	spin1_callback_on(FRPL_PACKET_RECEIVED, hFR, PRIORITY_FR);
	spin1_callback_on(DMA_TRANSFER_DONE, hDMADone, PRIORITY_DMA);
	spin1_start(SYNC_NOWAIT);
}
