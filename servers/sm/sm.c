/* Security Monitor Server.  */

	
#include "inc.h"	/* include master header file */

/* Allocate space for the global variables. */
int callnr;		/* system call number */
int who_e;		/* caller's proc number */

extern int errno;	/* error number set by system library */

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (int argc, char **argv)		);
FORWARD _PROTOTYPE(void exit_server, (void)				);
FORWARD _PROTOTYPE(void sig_handler, (void)				);
FORWARD _PROTOTYPE(void get_work, (message *m_ptr)			);
FORWARD _PROTOTYPE(void reply, (int whom, message *m_ptr)		);


/*===========================================================================*
 *				main                                         *
 *===========================================================================*/
PUBLIC int main(int argc, char **argv)
{

  message m, *m_ptr;
  int result, rcode;                 
  sigset_t sigset;
  int cmd, i2, i3;
  void *p1, *p2, *p3;

  /* Initialize the server, then go to work. */
  init_server(argc, argv);
   m_ptr = &m;

   /* Main loop - get work and do it, forever. */         
  while (TRUE) {              

      /* Wait for incoming message, sets 'callnr' and 'who'. */
      get_work(&m);

    switch (callnr) {
		case GETPID:
			result = _relay(PM_PROC_NR, m_ptr, who_e);
			if( result)
				printf("relay result=%d\n", result);
			result = EDONTREPLY;
			break;
		default: 
			result = EINVAL;
			break;
    }

    /* Finally send reply message, unless disabled. */
    if (result != EDONTREPLY) {
        m.m_type = result;  		/* build reply message */
		reply(who_e, &m);		/* send it away */
    }
  }
  return(OK);				/* shouldn't come here */
}

/*===========================================================================*
 *				 init_server                                 *
 *===========================================================================*/
PRIVATE void init_server(int argc, char **argv)
{
/* Initialize the Security Monitor  server. */
  int i, s;
  struct sigaction sigact;
	int sm_vmid;
    
	sm_vmid = _mhdebug("SECURITY MONITOR SERVER RUNNING !!!", 0);
   	sm_vmid = _mhdebug("SM VMID=", sm_vmid);

  /* Install signal handler. Ask PM to transform signal into message. */
  sigact.sa_handler = SIG_MESS;
  sigact.sa_mask = ~0;			/* block all other signals */
  sigact.sa_flags = 0;			/* default behaviour */
  if (sigaction(SIGTERM, &sigact, NULL) < 0) 
      report("SM","warning, sigaction() failed", errno);
}

/*===========================================================================*
 *				 sig_handler                                 *
 *===========================================================================*/
PRIVATE void sig_handler()
{
/* Signal handler. */
  sigset_t sigset;
  int sig;

  /* Try to obtain signal set from PM. */
  if (getsigset(&sigset) != 0) return;

  /* Check for known signals. */
  if (sigismember(&sigset, SIGTERM)) {
      exit_server();
  }
}

/*===========================================================================*
 *				exit_server                                  *
 *===========================================================================*/
PRIVATE void exit_server()
{
/* Shut down the information service. */

  /* Done. Now exit. */
  exit(0);
}

/*===========================================================================*
 *				get_work                                     *
 *===========================================================================*/
PRIVATE void get_work(m_ptr)
message *m_ptr;				/* message buffer */
{
    int status = 0;
    status = receive(ANY, m_ptr);   /* this blocks until message arrives */
    if (OK != status)
        panic("SM","failed to receive message!", status);
    who_e = m_ptr->m_source;        /* message arrived! set sender */
	callnr = m_ptr->m_type;       /* set function call number */

	printf(MSG1_FORMAT, MSG1_FIELDS(m_ptr)); 

}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE void reply(who_e, m_ptr)
int who_e;                           	/* destination */
message *m_ptr;				/* message buffer */
{
    int s;
    s = send(who_e, m_ptr);    /* send the message */
    if (OK != s)
        panic("DS", "unable to send reply!", s);
}

