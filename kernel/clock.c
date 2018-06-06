/* This file contains the clock task, which handles time related functions.
 * Important events that are handled by the CLOCK include setting and 
 * monitoring alarm timers and deciding when to (re)schedule processes. 
 * The CLOCK offers a direct interface to kernel processes. System services 
 * can access its services through system calls, such as sys_setalarm(). The
 * CLOCK task thus is hidden from the outside world.  
 *
 * Changes:
 *   Oct 08, 2005   reordering and comment editing (A. S. Woodhull)
 *   Mar 18, 2004   clock interface moved to SYSTEM task (Jorrit N. Herder) 
 *   Sep 30, 2004   source code documentation updated  (Jorrit N. Herder)
 *   Sep 24, 2004   redesigned alarm timers  (Jorrit N. Herder)
 *
 * The function do_clocktick() is triggered by the clock's interrupt 
 * handler when a watchdog timer has expired or a process must be scheduled. 
 *
 * In addition to the main clock_task() entry point, which starts the main 
 * loop, there are several other minor entry points:
 *   clock_stop:	called just before MINIX shutdown
 *   get_uptime:	get realtime since boot in clock ticks
 *   set_timer:		set a watchdog timer (+)
 *   reset_timer:	reset a watchdog timer (+)
 *   read_clock:	read the counter of channel 0 of the 8253A timer
 *
 * (+) The CLOCK task keeps tracks of watchdog timers for the entire kernel.
 * The watchdog functions of expired timers are executed in do_clocktick(). 
 * It is crucial that watchdog functions not block, or the CLOCK task may
 * be blocked. Do not send() a message when the receiver is not expecting it.
 * Instead, notify(), which always returns, should be used. 
 */

#include "kernel.h"
#include "proc.h"
#include <signal.h>
#include <minix/com.h>

#ifdef MHYPER
#include <minix/endpoint.h>
#endif /* MHYPER */

/* Function prototype for PRIVATE functions. */ 
FORWARD _PROTOTYPE( void init_clock, (void) );
FORWARD _PROTOTYPE( int clock_handler, (irq_hook_t *hook) );
FORWARD _PROTOTYPE( int do_clocktick, (message *m_ptr) );
FORWARD _PROTOTYPE( void load_update, (void));

/* Clock parameters. */
#define COUNTER_FREQ (2*TIMER_FREQ) /* counter frequency using square wave */
#define LATCH_COUNT     0x00	/* cc00xxxx, c = channel, x = any */
#define SQUARE_WAVE     0x36	/* ccaammmb, a = access, m = mode, b = BCD */
				/*   11x11, 11 = LSB then MSB, x11 = sq wave */
#define TIMER_COUNT ((unsigned) (TIMER_FREQ/HZ)) /* initial value for counter*/
#define TIMER_FREQ  1193182L	/* clock frequency for timer in PC and AT */

#define CLOCK_ACK_BIT	0x80	/* PS/2 clock interrupt acknowledge bit */

/* The CLOCK's timers queue. The functions in <timers.h> operate on this. 
 * Each system process possesses a single synchronous alarm timer. If other 
 * kernel parts want to use additional timers, they must declare their own 
 * persistent (static) timer structure, which can be passed to the clock
 * via (re)set_timer().
 * When a timer expires its watchdog function is run by the CLOCK task. 
 */
PRIVATE timer_t *clock_timers;		/* queue of CLOCK timers */
PRIVATE clock_t next_timeout;		/* realtime that next timer expires */

/* The time is incremented by the interrupt handler on each clock tick. */
PRIVATE clock_t realtime;		/* real time clock */
PRIVATE irq_hook_t clock_hook;		/* interrupt handler hook */

/* Variables para medir comportamiento del planificador*/
#ifdef MHYPER
PRIVATE int counter[NR_VMS][3];
PRIVATE int idle_counter;
PRIVATE int r_ticks;
PRIVATE int metric_i;	/* Which position to write in metric structure */

#define	MHSCHED_VERSION2	2
#endif /* MHYPER */

/*===========================================================================*
 *				clock_task				     *
 *===========================================================================*/
PUBLIC void clock_task()
{
/* Main program of clock task. If the call is not HARD_INT it is an error.
 */
  message m;			/* message buffer for both input and output */
  int result;			/* result returned by the handler */

#ifdef MHYPER
  running_vm = VM0;
#endif /*  MHYPER */
  init_clock();			/* initialize clock task */

  /* Main loop of the clock task.  Get work, process it. Never reply. */
  while (TRUE) {

      /* Go get a message. */
      receive(ANY, &m);	

      /* Handle the request. Only clock ticks are expected. */
      switch (m.m_type) {
      case HARD_INT:
          result = do_clocktick(&m);	/* handle clock tick */
          break;
      default:				/* illegal request type */
          kprintf("CLOCK: illegal request %d from %d.\n", m.m_type,m.m_source);
      }
  }
}

/*===========================================================================*
 *				do_clocktick				     *
 *===========================================================================*/
PRIVATE int do_clocktick(m_ptr)
message *m_ptr;				/* pointer to request message */
{
/* Despite its name, this routine is not called on every clock tick. It
 * is called on those clock ticks when a lot of work needs to be done.
 */

  /* A process used up a full quantum. The interrupt handler stored this
   * process in 'prev_ptr'.  First make sure that the process is not on the 
   * scheduling queues.  Then announce the process ready again. Since it has 
   * no more time left, it gets a new quantum and is inserted at the right 
   * place in the queues.  As a side-effect a new process will be scheduled.
   */ 
	/* refresh token buckets */
	int E_tokens, i, nx_vm, nvm_tokens;
	unsigned int bm;
	struct vm_struct *rvm, *nvm;		/* running/next virtual machine */

	
#ifdef MHYPER	
	/* check if time to change next_vm to avoid CPU monopolization */
	lock(0, "do_clocktick");
	if( switch_vm <= 0 ){
		nvm_tokens = 0;
		/* VM bitmap without VM0 */
		bm = VM_bitmap & ~((1 << VM0) | (1 << next_vm));
		nx_vm = next_vm;
		unlock(0);

		/* circular search of an active VM with tokens */
		while (bm != 0) {				
			nx_vm = (nx_vm == (NR_VMS-1))?(VM0+1):(nx_vm+1);
			if( bm & (1 << nx_vm)){
				nvm = &vm_hyper[nx_vm];
				if( nvm->vm_tokens > 0) {/* MATCH !!  with bm != 0 */
					if( next_vm != nx_vm) { /* new next_vm */
						next_vm = nx_vm;
						nvm_tokens = (nvm->vm_tokens > TICKS_TO_SWITCH)?TICKS_TO_SWITCH:nvm->vm_tokens;
					} 
					break; 
				}
				bm &= ~(1 << nx_vm);
			}
		}

		lock(0, "do_clocktick");
		if( bm == 0 ) {	/* there is not new next_vm */
			next_vm = VM0;
			nvm_tokens = TICKS_TO_SWITCH;
		}	
		switch_vm = nvm_tokens;	/* Allocate tokens to the switching counter (0 tokens for VM0) */ 
	}

	if(vm_hyper[VM0].vm_tokens == MAXTOKENS){
		for(i = (VM0+1); i < NR_VMS; i++){
			rvm = &vm_hyper[i];
			if(rvm->vm_flags == VM_RUNNING) {
				rvm->vm_tokens = rvm->vm_bsize;
				vm_hyper[VM0].vm_tokens -= rvm->vm_tokens;
			}
		}
	}

	unlock(0);
#endif /* MHYPER	*/

	   
	/* preempt current process */
	if ( (prev_ptr->p_ticks_left <= 0) && 
		(priv(prev_ptr)->s_flags & PREEMPTIBLE) ) {
		lock_dequeue(prev_ptr);		/* take it off the queues */
		lock_enqueue(prev_ptr);		/* and reinsert it again */ 
	}

	/* Check if a clock timer expired and run its watchdog function. */
	if (next_timeout <= realtime) { 
		tmrs_exptimers(&clock_timers, realtime, NULL);
		next_timeout = clock_timers == NULL ? 
			TMR_NEVER : clock_timers->tmr_exp_time;
	}

	/* Inhibit sending a reply. */
	return(EDONTREPLY);
}

/*===========================================================================*
 *				init_clock				     *
 *===========================================================================*/
PRIVATE void init_clock()
{
  int i,j;
	
  /* Initialize the CLOCK's interrupt hook. */
  clock_hook.proc_nr_e = CLOCK;
#ifdef MHYPER
  clock_hook.vmid = VM0;
#endif /*  MHYPER */
  /* Initialize channel 0 of the 8253A timer to, e.g., 60 Hz, and register
   * the CLOCK task's interrupt handler to be run on every clock tick. 
   */
  outb(TIMER_MODE, SQUARE_WAVE);	/* set timer to run continuously */
  outb(TIMER0, TIMER_COUNT);		/* load timer low byte */
  outb(TIMER0, TIMER_COUNT >> 8);	/* load timer high byte */
  put_irq_handler(&clock_hook, CLOCK_IRQ, clock_handler);
  enable_irq(&clock_hook);		/* ready for clock interrupts */

  /* Set a watchdog timer to periodically balance the scheduling queues. */
  balance_queues(NULL);			/* side-effect sets new timer */

#ifdef MHYPER
	/* Counter to measure scheduler performance */
	for(i = 0; i < NR_VMS; i++){
		counter[i][0] = counter[i][1] = counter[i][2] = 0;
	}
	r_ticks = 0;
	idle_counter = 0;

	for(j = 0; j < 1024; j++){
	  for(i = 0; i < NR_VMS; i++){
	    metric[j].kernel[i] = 0;
	    metric[j].task[i] = 0;
	    metric[j].proc[i] = 0;
	  }
	  metric[j].timestamp = 0;
	  metric[j].idle = 0;
	}
	metric_i = 0;

#endif /* MHYPER */
}

/*===========================================================================*
 *				clock_stop				     *
 *===========================================================================*/
PUBLIC void clock_stop()
{
/* Reset the clock to the BIOS rate. (For rebooting.) */
  outb(TIMER_MODE, 0x36);
  outb(TIMER0, 0);
  outb(TIMER0, 0);
}

/*===========================================================================*
 *				clock_handler				     *
 *===========================================================================*/
PRIVATE int clock_handler(hook)
irq_hook_t *hook;
{
/* This executes on each clock tick (i.e., every time the timer chip generates 
 * an interrupt). It does a little bit of work so the clock task does not have 
 * to be called on every tick.  The clock task is called when:
 *
 *	(1) the scheduling quantum of the running process has expired, or
 *	(2) a timer has expired and the watchdog function should be run.
 *
 * Many global global and static variables are accessed here.  The safety of
 * this must be justified. All scheduling and message passing code acquires a 
 * lock by temporarily disabling interrupts, so no conflicts with calls from 
 * the task level can occur. Furthermore, interrupts are not reentrant, the 
 * interrupt handler cannot be bothered by other interrupts.
 * 
 * Variables that are updated in the clock's interrupt handler:
 *	lost_ticks:
 *		Clock ticks counted outside the clock task. This for example
 *		is used when the boot monitor processes a real mode interrupt.
 * 	realtime:
 * 		The current uptime is incremented with all outstanding ticks.
 *	proc_ptr, bill_ptr:
 *		These are used for accounting.  It does not matter if proc.c
 *		is changing them, provided they are always valid pointers,
 *		since at worst the previous process would be billed.
 */
  register unsigned ticks;

#ifdef MHYPER
 	struct proc *ld_ptr;
	struct vm_struct *rvm;		/* running virtual machine */
	int ld_ep, ld_nr;
	int i, saved_vm, vmid;
	unsigned int bm;
	
	/* DEAL WITH TOKENS  */
	vmid = proc_ptr->p_vmid; 
	rvm = &vm_hyper[vmid];
	
	if( rvm->vm_tokens > 0) {
		rvm->vm_tokens--; 
		vm_hyper[VM0].vm_tokens++;
	}

	/* Deal with NEXT_VM */
	if( vmid == next_vm ) 
		switch_vm--;
	
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* El vuelco de estadisticas no deberia hacerse en tiempo de INTERRUPCION */
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#ifdef MHSCHED_STATS 
	/* Se mide como se comporta el planificador
	 * Cada tick se cuenta que VM se esta ejecutando y si es task o proceso
	 */

	if(proc_ptr->p_priority == IDLE_Q)
		idle_counter ++;
	else if(MH_LEVEL(proc_ptr->p_misc_flags) == KERNEL_LEVEL)
		counter[proc_ptr->p_vmid][0] ++;
	else if(MH_LEVEL(proc_ptr->p_misc_flags) == TASK_LEVEL)
		counter[proc_ptr->p_vmid][1] ++;
	else 
		counter[proc_ptr->p_vmid][2] ++;
	
	r_ticks ++;

#define  SCHED_STATS_DUMP	(5 * HZ)

	if(r_ticks >= SCHED_STATS_DUMP){
		/* To serial port */
		MHDEBUG("\nPlanificador:\n");
		MHDEBUG("Time %ld\n", realtime); 
		for(i = 0; i < NR_VMS; i++){
			MHDEBUG("KERN VM %d: %d\n", i, counter[i][0]);
			MHDEBUG("TASK VM %d: %d\n", i, counter[i][1]);
			MHDEBUG("PROC VM %d: %d\n", i, counter[i][2]);
		}
		MHDEBUG("IDLE: %d\n", idle_counter);
		
		/* A la estructura metric */
		if(metric_i < NR_METRICS){
		  for(i = 0; i < NR_VMS; i++){
			metric[metric_i].kernel[i] = counter[i][0];
		    metric[metric_i].task[i] = counter[i][1];
		    metric[metric_i].proc[i] = counter[i][2];
		  }
		  metric[metric_i].timestamp = realtime;
		  metric[metric_i].idle = idle_counter;
		  metric_i++;
		  if(metric_i >= NR_METRICS) metric_i = 0;
		}
	  
		/* Todo a cero */
		r_ticks = 0;
		for(i = 0; i < NR_VMS; i++){
			counter[i][0] = counter[i][1] = counter[i][2] = 0;
		}
		idle_counter = 0;
	}
#endif /* MHSCHED_STATS */

#endif /* MHYPER */

  /* Acknowledge the PS/2 clock interrupt. */
  if (machine.ps_mca) outb(PORT_B, inb(PORT_B) | CLOCK_ACK_BIT);

  /* Get number of ticks and update realtime. */
  ticks = lost_ticks + 1;
  lost_ticks = 0;
  realtime += ticks;

  /* Update user and system accounting times. Charge the current process for
   * user time. If the current process is not billable, that is, if a non-user
   * process is running, charge the billable process for system time as well.
   * Thus the unbillable process' user time is the billable user's system time.
   */
#ifdef MHYPER
	if( proc_ptr->p_nr == SYSTEM) {
		vm_hyper[proc_ptr->p_vmid].vm_proc[SYSTEM+NR_TASKS].p_user_time += ticks;
	} else { 	
		proc_ptr->p_user_time += ticks;
	}
	vm_hyper[proc_ptr->p_vmid].vm_time++;

	/* ONLY FOR VM0: charge the ticks consummed from other VM to the Loader of its VM */
	if(proc_ptr->p_vmid > VM0 &&  proc_ptr->p_nr > HARDWARE) {
		ld_ep = vm_hyper[proc_ptr->p_vmid].vm_VMM.vmm_endpoint;	
		ld_nr = _ENDPOINT_P(ld_ep);
		ld_ptr = proc_addr_vm(VM0, ld_nr);
		ld_ptr->p_user_time += ticks; 
	}
#else /*  MHYPER */
  proc_ptr->p_user_time += ticks;
#endif /*  MHYPER */ 

  if (priv(proc_ptr)->s_flags & PREEMPTIBLE) {
      proc_ptr->p_ticks_left -= ticks;
  }

  if (! (priv(proc_ptr)->s_flags & BILLABLE)) {
      bill_ptr->p_sys_time += ticks;
      bill_ptr->p_ticks_left -= ticks;
  }

  /* Update load average. */
  load_update();

  /* Check if do_clocktick() must be called. Done for alarms and scheduling.
   * Some processes, such as the kernel tasks, cannot be preempted. 
   */ 
   if ((next_timeout <= realtime) 		|| 
		(proc_ptr->p_ticks_left <= 0) 	||
		(switch_vm <= 0)		||
		(vm_hyper[VM0].vm_tokens == MAXTOKENS)	){
		prev_ptr = proc_ptr;		/* store running process */
#ifdef MHYPER
		lock(0, "notify");
		saved_vm = proc_ptr->p_vmid;
		running_vm = VM0;
		raw_notify(HARDWARE, CLOCK); 
		running_vm = saved_vm;
		unlock(0);
#endif /*  MHYPER */ 
	  
  }

  return(1);					/* reenable interrupts */
}

/*===========================================================================*
 *				get_uptime				     *
 *===========================================================================*/
PUBLIC clock_t get_uptime()
{
/* Get and return the current clock uptime in ticks. */
#ifdef MHYPER
#ifdef IPC_DBG
	if( running_vm != VM0)
		MHDEBUG("get_uptime realtime=%d vm_uptime=%d uptime=%d\n",
			realtime, 
			vm_hyper[running_vm].vm_uptime, 
			(realtime - vm_hyper[running_vm].vm_uptime));
#endif /* IPC_DBG */

	return(realtime - vm_hyper[running_vm].vm_uptime); 
#else /*  MHYPER */
	return(realtime);
#endif /*  MHYPER */
}

/*===========================================================================*
 *				set_timer				     *
 *===========================================================================*/
PUBLIC void set_timer(tp, exp_time, watchdog)
struct timer *tp;		/* pointer to timer structure */
clock_t exp_time;		/* expiration realtime */
tmr_func_t watchdog;		/* watchdog to be called */
{
/* Insert the new timer in the active timers list. Always update the 
 * next timeout time by setting it to the front of the active list.
 */
  tmrs_settimer(&clock_timers, tp, exp_time, watchdog, NULL);
  next_timeout = clock_timers->tmr_exp_time;
}

/*===========================================================================*
 *				reset_timer				     *
 *===========================================================================*/
PUBLIC void reset_timer(tp)
struct timer *tp;		/* pointer to timer structure */
{
/* The timer pointed to by 'tp' is no longer needed. Remove it from both the
 * active and expired lists. Always update the next timeout time by setting
 * it to the front of the active list.
 */
  tmrs_clrtimer(&clock_timers, tp, NULL);
  next_timeout = (clock_timers == NULL) ? 
	TMR_NEVER : clock_timers->tmr_exp_time;
}

/*===========================================================================*
 *				read_clock				     *
 *===========================================================================*/
PUBLIC unsigned long read_clock()
{
/* Read the counter of channel 0 of the 8253A timer.  This counter counts
 * down at a rate of TIMER_FREQ and restarts at TIMER_COUNT-1 when it
 * reaches zero. A hardware interrupt (clock tick) occurs when the counter
 * gets to zero and restarts its cycle.  
 */
  unsigned count;

  outb(TIMER_MODE, LATCH_COUNT);
  count = inb(TIMER0);
  count |= (inb(TIMER0) << 8);
  
  return count;
}

/*===========================================================================*
 *				load_update				     * 
 *===========================================================================*/
PRIVATE void load_update(void)
{
	u16_t slot;
	int enqueued = -1, q;	/* -1: special compensation for IDLE. */
	struct proc *p;

	/* Load average data is stored as a list of numbers in a circular
	 * buffer. Each slot accumulates _LOAD_UNIT_SECS of samples of
	 * the number of runnable processes. Computations can then
	 * be made of the load average over variable periods, in the
	 * user library (see getloadavg(3)).
	 */
	slot = (realtime / HZ / _LOAD_UNIT_SECS) % _LOAD_HISTORY;
	if(slot != kloadinfo.proc_last_slot) {
		kloadinfo.proc_load_history[slot] = 0;
		kloadinfo.proc_last_slot = slot;
	}

	/* Cumulation. How many processes are ready now? */
	for(q = 0; q < NR_SCHED_QUEUES; q++)
		for(p = rdy_head[q]; p != NIL_PROC; p = p->p_nextready)
			enqueued++;

	kloadinfo.proc_load_history[slot] += enqueued;

	/* Up-to-dateness. */
	kloadinfo.last_clock = realtime;
}

