#include <spin1_api.h>

#define TIMER_TICK_PERIOD_US	1000000	// in milliseconds
#define TIMER_PRIORITY		0

uint myAppID;
uint myCoreID;

void hTimer(uint tick, uint null)
{
    io_printf(IO_STD, "Hello from core-%d with appID-%d\n", sark_core_id(), sark_app_id());
}

void c_main (void)
{
    uint myAppID, myCoreID;
    io_printf(IO_STD, "Starting of c_main at %x\n\n", c_main); sark_delay_us(1000);

    spin1_set_timer_tick(TIMER_TICK_PERIOD_US);
    spin1_callback_on(TIMER_TICK, hTimer, TIMER_PRIORITY);
    spin1_start(SYNC_NOWAIT);
}
