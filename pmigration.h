#ifndef PMIGRATION_H
#define PMIGRATION_H

#include <spinnaker.h>

#define TIMER_TICK_PERIOD_US	1000000		// in milliseconds
#define DEBUG_LEVEL		1		// 0 = no io_printf, 1 = with io_printf

#define USE_SARK		0
#define USE_API			1
#define USE_MCPL		2
#define USE_SDP			3
#define DMA_TRANSFER_ITCM_ID	14		// magic number. Note, the DMA transfer ID is only 6-bits
#define DMA_TRANSFER_DTCM_ID	41
#define DMA_DWORD		1		// 0 = word, 1 = double word
#define DMA_BURST		16		// valid values: 1, 2, 4, 8, 16. So, let's try as fast as possible
#define DEF_SDP_TIMEOUT		10

/* for pingpong */
#define DMA_PRIORITY		-1
#define MCPL_PRIORITY		0
#define SDP_PRIORITY		0
#define TIMER_PRIORITY		1
#define BACKUP_PRIORITY		1		// if priority <=0, it produces weird behavior on spin1_schedule_callback
#define PINGER_CORE		2
#define PONGER_CORE		5

/********************************************************************************************************************************************************************/
/* for stub */
#define STUB_TRIGGER_CMD	0xFA95		// if using SDP, magic number, just a random
#define STUB_TRIGGER_SEQ	0xBF3E
#define STUB_APP_ID		254
#define STUB_SDP_PORT		7
#define APP_TCM_SIZE		0x13F00		// = APP_ITCM_SIZE + APP_DTCM_SIZE --> lihat di stub.c
/*-------------- With debug then size is different ---------------*/
#define RESERVED_ITCM_TOP_SIZE	0x100

#define STUB_DATA_BASE			0
#define STUB_DATA_SIZE			0x800			// 2KB is enough?
#define STUB_STACK_TOP			0x00401400;		/* OK: 5KB above the DTCM_BASE, where the data is only 1KB */
								/* it's supposed to be 0x800 + 0x800 = 0x1000, but somehow it doesn't work! */
								/* but it works with additional 1K, so stack top address = 0x800 + 0x800 + 0x400 = 0x1400 */
#define STUB_STACK_SIZE			0x800;		        /* 2KB of stack */

// the following definitions must match the linker script!!!
#if(DEBUG_LEVEL==0)
#define STUB_PROG_SIZE
#define STUB_PROG_BASE

#define APP_PROG_SIZE
#define APP_PROG_BASE
#define APP_DTCM_BASE

#define APP_DATA_SIZE
#define APP_STACK_SIZE
#define APP_STACK_TOP

#else
#define STUB_ITCM_SIZE			0x2000			// = 8192 (8KB), the current stub in aplx format
#define STUB_ITCM_BASE			0x5F00			// 0x7F00 - 0x2000 = 5F00, tidak termasuk reserved area di pucuk ITCM

#define APP_ITCM_SIZE			0x5F00			// = 0x8000 - 0x2000 - 0x100 = ITCM_SIZE - STUB_PROG_SIZE - RESERVED_ITCM_TOP_SIZE
#define APP_ITCM_BASE			0x00000000		// for apps, the ITCM base is the same with normal ITCM (0x00000000)
#define APP_DTCM_BASE			0x00401800		// normal DTCM_BASE 0x00400000
												// in this case, I put 1K above the sub_stack_top (0x1400 + 0x400)
#define APP_DTCM_SIZE			0xE000			// = 0x40f800 - 0x401800 = LOKASI_DIBAWAH_STACK - LOKASI_AWAL_APP_DTCM
#define APP_STACK_SIZE			0x800			// the same value as usual
#define APP_STACK_TOP			0x00410000

#endif
/*----------------------------------------------------------------*/
/********************************************************************************************************************************************************************/

/* for supervisor */
// SDP port usage:
#define PORT_FOR_TCM
#define SUPERVISOR_APP_ID	255
#define NEW_PINGER_CORE		7

#define MAX_AVAIL_CORE		16

typedef struct app_stub		// application holder
{
    ushort coreID;
    ushort appID;
    uint *tcm_addr;
    //uint itcm_addr;
    //uint dtcm_addr;
    uint *restart_addr;
} app_stub_t;

#endif

/* Note:
 * - Use Key "ccssiiyy" to send to a destination-core cc
 *   ss can be used as the source (the sending core) of the packet
 *   ii is the application id of the source
 *   y can be used to indicate for types of payload is carried on, eq:
 *   y = 1 -> about ITCM Storage in SDRAM, if send by app-core to supv-core, it request the address
 *            but if sent by supv-core to app-core or stub-core, it contains the TCM storage in SDRAM
 *   y = 2 -> about AJMP, app-core send to supv-core for normal update,
 *	      supv-core send to stub-core to trigger the migration
 * - In general, use Key "ccxxxxxx" to send to core-cc
 *   (cc is the destination core)
 * */

/* key allocations */
// default entry key, will be allocated by supv-core:
#define MASK_CORE_SELECTOR	0x1F000000
// example keys:
// #define KEY_DEF_TO_SUPV	0x11000000
// #define RTR_DEF_TO_SUPV	0x00800000	// (1 << 6+17)
// #define KEY_DEF_TO_CORE1	0x01000000
// #define RTR_DEF_TO_CORE1	0x00000080
// etc.

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
