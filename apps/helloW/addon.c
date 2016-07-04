#include <sark.h>

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

