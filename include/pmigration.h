#ifndef PMIGRATION_H
#define PMIGRATION_H

#include <spinnaker.h>

#define TIMER_TICK_PERIOD_US	1000000		// in milliseconds
#define DEBUG_LEVEL		1		// 0 = no io_printf, 1 = with io_printf

#define USE_SARK		0
#define USE_API			1
#define DMA_TRANSFER_ITCM_ID	14		// magic number. Note, the DMA transfer ID is only 6-bits
#define DMA_TRANSFER_DTCM_ID	41
#define DMA_DWORD		1		// 0 = word, 1 = double word
#define DMA_BURST		16		// valid values: 1, 2, 4, 8, 16. So, let's try as fast as possible
#define DEF_SDP_TIMEOUT		10

#define PRIORITY_DMA		1
#define PRIORITY_FR		0	// the highest priority before FIQ. TODO: should we put in FIQ?
#define PRIORITY_SDP		3	// useful for demonstration only (triggering?)
#define PRIORITY_TIMER		2	// useful for checkpointing?
// NOTE:for all scheduled callback, if priority <=0, it produces weird behavior on spin1_schedule_callback

/**************************************************** Linker Setup ******************************************************/
// the following definitions must match the linker scripts: pmagent.lnk, app.lnk (supv uses normal sark.lnk)
#define RESERVED_ITCM_TOP_SIZE		0x100			// this is reserved area on top of 32KB ITCM used by sark for app loading & system function
#define STUB_DATA_BASE			0x00000000
#define STUB_DATA_SIZE			0x800			// 2KB is enough?
#define STUB_STACK_TOP			0x00401400;		// OK: 5KB above the DTCM_BASE, where the data is only 1KB
								// it's supposed to be 0x800 + 0x800 = 0x1000, but somehow it doesn't work!
								// but it works with additional 1K, so stack top address = 0x800 + 0x800 + 0x400 = 0x1400
#define STUB_STACK_SIZE			0x800;			// 2KB of stack

#define STUB_PROG_SIZE			0x2000			// = 8192 (8KB), the current stub in aplx format
#define STUB_PROG_BASE			0x5F00

#define APP_PROG_SIZE			0x5F00			// = 0x8000 - 0x2000 - 0x100 = ITCM_SIZE - STUB_PROG_SIZE - RESERVED_ITCM_TOP_SIZE
#define APP_PROG_BASE			0x00000000
#define APP_DTCM_BASE			0x00401800		// normal DTCM_BASE 0x00400000
								// but here, I put 1K above the sub_stack_top (0x1400 + 0x400)
#define APP_DATA_SIZE			0xE000			// = 0x40f800 - 0x401800 = LOKASI_DIBAWAH_STACK - LOKASI_AWAL_APP_DTCM
#define APP_STACK_SIZE			0x800			// the same value as usual
#define APP_STACK_TOP			0x00410000
/*************************************************************************************************************************/

/* for supervisor */
#define SUPERVISOR_APP_ID	255
#define MAX_AVAIL_CORE		16

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

#endif

/* This is the old MCPL-based signalling name
// key modifier (for every "00", replace according to ccssiiyy format above):
#define KEY_APP_ASK_TCMSTG	0x11000001	// an app core send a request for TCM storage in SDRAM
						// replace with "ssii" accordingly
#define KEY_SUPV_SEND_TCMSTG	0x0011FF01	// supv-core send the TCM storage address in SDRAM,
						// replace 00 with app-core or stub-core
						// app-core and STUB-core must infer ITCM & DTCM accordingly
#define KEY_APP_SEND_AJMP	0x11000002	// AJMP is contained in the payload
						// replace with "ssii" accordingly
#define KEY_SUPV_SEND_AJMP	0x0011FF02	// replace 00 with stub-core
						// replace FF with correct appID
#define KEY_STUB_READY2JMP	0x11000003	// stub send "ready to jump" signal to supervisor
#define KEY_SUPV_SEND_STOP	0x0011FFFF	// supv-core "kills" the app-core
*/

/* FR-based signalling name
 * for stub, FR might contains "key" field. Use it for signaling
 */
#define KEY_APP_ASK_TCMSTG		0x45FE		// a pmagent send a request for TCM storage in SDRAM
#define KEY_SUPV_REPLY_ITCMSTG		0x46FE		// supv reply with ITCM storage in SDRAM
#define KEY_SUPV_REPLY_DTCMSTG		0x47FE		// supv reply with DTCM storage in SDRAM
#define KEY_APP_SEND_AJMP		0x48FE		// regularly (during checkpointing), app cores will send this start jump address
#define KEY_SUPV_TRIGGER_ITCMSTG	0x1234		// supv send ITCM location to a pmagent
#define KEY_SUPV_TRIGGER_DTCMSTG	0x5678		// supv send DTCM location to a pmagent

// for application examples and demonstration purpose:
#define PINGER_CORE		2
#define PONGER_CORE		5
#define NEW_PINGER_CORE		7
#define DEMO_TRIGGERING_PORT	7

