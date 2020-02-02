/* Copyright 2019 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <metal/cpu.h>
#include <metal/lock.h>

#define NUM_ITERATIONS 10
#include <stdlib.h>
#include <stdint.h>
#include <metal/machine/platform.h>


#define REG32(p, i)	((p)[(i) >> 2])

#define GPIO_INPUT_VAL METAL_SIFIVE_GPIO0_VALUE
#define GPIO_OUTPUT_VAL METAL_SIFIVE_GPIO0_PORT

volatile uint32_t * const gpio0 = (void *)(METAL_SIFIVE_GPIO0_0_BASE_ADDRESS);
volatile uint32_t * const gpio1 = (void *)(METAL_SIFIVE_GPIO0_1_BASE_ADDRESS);
volatile uint32_t * const gpio2 = (void *)(METAL_SIFIVE_GPIO0_2_BASE_ADDRESS);
volatile uint32_t * const gpio3 = (void *)(METAL_SIFIVE_GPIO0_3_BASE_ADDRESS);

void num2bcd(int num, char * array)
{
	char bcd2seg[]={0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xf8, 0x80, 0x90, 0xff};
	int i;
	for(i=0;i<8;i++)
		array[i]=bcd2seg[10];
	if(num==0)
		array[0]=bcd2seg[0];
	else
	{
		i=num;
		while(i)
		{
			int r = i % 10;
			i = i / 10;
			*array++ = bcd2seg[r];
		}
	}
}

unsigned int x=0xf,count=10, display=1000000;


METAL_LOCK_DECLARE(my_lock);

int main(void);
int other_main();

/* This flag tells the secondary harts when to start to make sure
 * that they wait until the lock is initialized */
volatile int _start_other = 0;

/* This is a count of the number of harts who have executed their main function */
volatile int checkin_count = 0;

/* The secondary_main() function can be redefined to start execution
 * on secondary harts. We redefine it here to cause harts with
 * hartid != 0 to execute other_main() */
int secondary_main(void) {
	int hartid = metal_cpu_get_current_hartid();
	if(hartid == 0) {
	        REG32(gpio3, GPIO_OUTPUT_VAL) = 0xFFFF;
		int rc = metal_lock_init(&my_lock);
		if(rc != 0) {
			exit(1);
		}

		/* Ensure that the lock is initialized before any readers of
		 * _start_other */
		__asm__ ("fence rw,w"); /* Release semantics */

		_start_other = 1;

		return main();
	} else {
		return other_main(hartid);
	}
}

int main(void) 
{
	int i, j;
	int turn=0, iter=0;
	while(1)
	{
		for(j=0;j<1000;j++)
		{
			for(i=0,iter=0;i<1000;i++)
			{
			  iter++;
			  REG32(gpio1, GPIO_OUTPUT_VAL)=iter;
			  if(iter == 1000)
			  {
			    turn++;
			    if(turn==8)	turn=0;
			  }
			}
		}
			
		if(count==16)
		{
			count=0;
			x=1;
		}
		else
		{
			x=x << 1;
			count++;
		}
		metal_lock_take(&my_lock);
		REG32(gpio0, GPIO_OUTPUT_VAL) = x & REG32(gpio1, GPIO_INPUT_VAL);
		metal_lock_give(&my_lock);
	}

	return 0;
}

int other_main(int hartid) {
	while(!_start_other) ;

	char bcdarray[8];
	
	int turn=0, iter=0;
	num2bcd(display,bcdarray);

	int i,j;
	
	while(1)
	{
		for(j=0;j<1000;j++)
		{
			for(i=0,iter=0;i<1000;i++)
			{
			  iter++;
			  REG32(gpio1, GPIO_OUTPUT_VAL)=iter;
			  if(iter == 1000)
			  {
			    REG32(gpio3, GPIO_OUTPUT_VAL)=(~(1 << turn) & 0xFF ) | (bcdarray[turn] << 8);
			    turn++;
			    if(turn==8)	turn=0;
			  }
			}
		}
		
		display++;
		if(display==100000000)
			display=0;
		num2bcd(display,bcdarray);
	}	



	return 0;
}
