/*
 * mhved.c
 *
 * This file contains a ethernet device driver for MHYPER Virtual Ethernet cards.
 *
 * The valid messages and their parameters are:
 *
 *   m_type       DL_PORT    DL_PROC   DL_COUNT   DL_MODE   DL_ADDR
 * |------------+----------+---------+----------+---------+---------|
 * | HARDINT    |          |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITE   | port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITEV  | port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READ    | port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READV   | port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_INIT    | port nr  | proc nr | mode     |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_GETSTAT | port nr  | proc nr |          |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_STOP    | port_nr  |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 *
 *
 * Adapted for MHYPER: Sep 22, 2014 by ppessolani@hotmail.com
 */

#define VERBOSE 0

#include "../drivers.h"

#include <minix/keymap.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <net/gen/eth_hdr.h>
#include <assert.h>

#include <minix/syslib.h>

#include "mhved.h"
/*#include "proc.h"*/
#define  TEST_BIT(var, bitnr)  (var & (1 << bitnr))

static ether_card_t ec_table[EC_PORT_NR_MAX][NR_VMS];
static int eth_tasknr= ANY;
static u16_t eth_ign_proto;

/* Configuration */
typedef struct ec_conf
{
  char *ec_envvar;
} ec_conf_t;

ec_conf_t ec_conf[]=    /* Card addresses */
{
        {  "MHVED0" },
};
/* General */
_PROTOTYPE( static void do_init, (message *mp)                          );
_PROTOTYPE( static void ec_init, (ether_card_t *ec)                     );
_PROTOTYPE( static void ec_confaddr, (ether_card_t *ec)                 );
_PROTOTYPE( static void ec_reinit, (ether_card_t *ec)                   );
_PROTOTYPE( static void conf_hw, (ether_card_t *ec)                     );
/*_PROTOTYPE( static int ec_handler, (irq_hook_t *hook)                   );*/
_PROTOTYPE( static void update_conf, (ether_card_t *ec, ec_conf_t *ecp) );
_PROTOTYPE( static void mess_reply, (message *req, message *reply)      );
_PROTOTYPE( static void do_int, (ether_card_t *ec)                      );
_PROTOTYPE( static void reply, 
	    (ether_card_t *ec, int err, int may_block)                  );
_PROTOTYPE( static void ec_reset, (ether_card_t *ec)                    );
_PROTOTYPE( static void ec_send, (ether_card_t *ec)                     );
_PROTOTYPE( static void ec_recv, (ether_card_t *ec)                     );
_PROTOTYPE( static void do_vwrite, 
	    (message *mp, int vectored)                   );
_PROTOTYPE( static void do_vread, (message *mp, int vectored)           );
_PROTOTYPE( static void get_userdata, 
	    (int user_proc, vir_bytes user_addr, 
	     vir_bytes count, void *loc_addr)                           );
_PROTOTYPE( static void ec_user2nic, 
	    (ether_card_t *dep, iovec_dat_t *iovp, 
	     vir_bytes offset, int nic_addr, 
	     vir_bytes count)                                           );
_PROTOTYPE( static void ec_nic2user, 
	    (ether_card_t *ec, int nic_addr, 
	     iovec_dat_t *iovp, vir_bytes offset, 
	     vir_bytes count)                                           );
_PROTOTYPE( static int calc_iovec_size, (iovec_dat_t *iovp)             );
_PROTOTYPE( static void ec_next_iovec, (iovec_dat_t *iovp)              );
_PROTOTYPE( static void do_getstat, (message *mp)                       );
_PROTOTYPE( static void put_userdata, 
	    (int user_proc,
	     vir_bytes user_addr, vir_bytes count, 
	     void *loc_addr)                                            );
_PROTOTYPE( static void do_stop, (message *mp)                          );
_PROTOTYPE( static void do_getname, (message *mp)                       );

_PROTOTYPE( static void mhved_dump, (void)				);
_PROTOTYPE( static void mhved_stop, (void)				);

/* probe+init MHVED cards */
_PROTOTYPE( static int mhved_probe, (ether_card_t *ec)                  );
_PROTOTYPE( static void mhved_init_card, (ether_card_t *ec)             );
_PROTOTYPE( static void  vnet_switch, (void ));
_PROTOTYPE( static void frame_tx2rx, (slot_t *s_ptr, buf_ring_t *rx_ptr, int frm_type));
_PROTOTYPE( slot_t *rmv_slot, (buf_ring_t *b_ptr, slot_t *s_ptr));

/* --- MHVED --- */
/* General */
#define Address                 unsigned long


#define ETH_FRAME_LEN           1518

#define MHVED_MUST_PAD          0x00000001
#define MHVED_ENABLE_AUTOSELECT 0x00000002
#define MHVED_SELECT_PHONELINE  0x00000004
#define MHVED_MUST_UNRESET      0x00000008


/* Use 2^4=16 {Rx,Tx} buffers */
#define MHVED_LOG_BUFFERS    4
#define RING_SIZE            (1 << (MHVED_LOG_BUFFERS))
#define RING_MOD_MASK        (RING_SIZE - 1)
#define RING_LEN_BITS        ((MHVED_LOG_BUFFERS) << 29)

#define MCAST_MASK 0x01

/* =============== global variables =============== */
static char *progname;
static int rx_slot_nr = 0;          /* Rx-slot number */
static int tx_slot_nr = 0;          /* Tx-slot number */
static int cur_tx_slot_nr = 0;      /* Tx-slot number */

#define   VMDEBUG(text,value)  		if (rqst_vmid > VM0) _mhdebug(text,value);

/* pool of slots */
PUBLIC	slot_t	eth_slot[NR_ETH_SLOTS];
#define NR_HEAD_SLOTS  	(2*NR_VMS*EC_PORT_NR_MAX)
PUBLIC	slot_t	head_slot[NR_HEAD_SLOTS];
PUBLIC	slot_t	free_head[1];

/* pool of frames */
PUBLIC  frame_t eth_frame[NR_ETH_FRAMES];

PUBLIC	buf_ring_t 	slot_free_list;
PUBLIC	frame_t 	*frame_free_list;

PUBLIC int rqst_vmid;
PUBLIC int vmcurrent;			/* currently VMID of active console  */
PUBLIC unsigned int vm_bitmap;  /* this bitmap keeps the VMs that MHVED serves as a promiscuous task*/

/* There are actually 4 sets of Locally Administered Address Ranges that can be used on your network 
    without fear of conflict, assuming no one else has assigned these on your network:
	x2-xx-xx-xx-xx-xx
	x6-xx-xx-xx-xx-xx
	xA-xx-xx-xx-xx-xx
	xE-xx-xx-xx-xx-xx
*/
PUBLIC ether_addr_t mac_addr_base = {06,05,04,03,02,01};
PUBLIC ether_addr_t mac_addr_bcast = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

#define l_head	l_next
#define l_tail	l_prev

/*===========================================================================*
 *                            init_cards                                     *
 *===========================================================================*/
void init_cards(void)
{
	int p, v;
	ether_card_t *ec_ptr;
	slot_t *s_ptr;

	_mhdebug("line",__LINE__);

	for( v=0; v < NR_VMS; v++){		
		for( p=0; p < EC_PORT_NR_MAX; p++){
			ec_ptr = &ec_table[p][v];

			/* invariants */
			strcpy(ec_ptr->port_name,"mhved_card#0");
			ec_ptr->port_name[11] += p;
			ec_ptr->ec_port = p;
			ec_ptr->ec_vmid = v;

			/* variable  */
			ec_ptr->flags = EC_DISABLED;
			ec_ptr->ec_rx.br_used = 0;
			ec_ptr->ec_tx.br_used = 0;

			s_ptr = &head_slot[(2*((v*EC_PORT_NR_MAX)+p))+0];
			ec_ptr->ec_rx.br_head = s_ptr;
			s_ptr->s_link.l_next = s_ptr;
			s_ptr->s_link.l_prev = s_ptr;
			s_ptr->s_flags= ETH_SLOT_HEAD;

			s_ptr = &head_slot[(2*((v*EC_PORT_NR_MAX)+p))+1];
			ec_ptr->ec_tx.br_head = s_ptr;
			s_ptr->s_link.l_next = s_ptr;
			s_ptr->s_link.l_prev = s_ptr;
			s_ptr->s_flags= ETH_SLOT_HEAD;

			MHDEBUG("init_cards: vmid=%d port=%d name=%s\n", v, p, ec_ptr->port_name);

			memset((void*)&ec_ptr->mac_address,0x00,MAC_ADDR_LEN);
		}
	}
}
/*===========================================================================*
 *                           ins_slot_tail                                   *
 * insert a slot at the tail of a buffer ring				*
 * return number of slots in ring or ERROR				*	
 *===========================================================================*/
int ins_slot_tail(buf_ring_t *b_ptr, slot_t *s_ptr)
{
	slot_t *h_ptr;

	h_ptr = b_ptr->br_head;

	s_ptr->s_link.l_prev = h_ptr->s_link.l_prev;
	h_ptr->s_link.l_prev->s_link.l_next = s_ptr;
	h_ptr->s_link.l_prev = s_ptr;
	s_ptr->s_link.l_next = h_ptr;

	b_ptr->br_used++;

	MHDEBUG("ins_slot_tail: s_nr=%d s_flags=%X \n", s_ptr->s_nr, s_ptr->s_flags);
	return(b_ptr->br_used);
}


/*===========================================================================*
 *                            init_slots                                     *
 *===========================================================================*/
void init_slots(void)
{
	int i;
	slot_t *s_ptr;

	_mhdebug("line",__LINE__);

	/* set the slot free list and its header */
	slot_free_list.br_used = 0;
	s_ptr = &free_head[0];
	slot_free_list.br_head = s_ptr;
	s_ptr->s_link.l_next = s_ptr;
	s_ptr->s_link.l_prev = s_ptr;
	s_ptr->s_flags= ETH_SLOT_HEAD;

	/* initialize each slot and insert it into the free list */
	for( i=0; i < NR_ETH_SLOTS ; i++){
		s_ptr = &eth_slot[i];
		s_ptr->s_nr  	= i;
		s_ptr->s_len	= 0;
		s_ptr->s_faddr	= NULL;
		s_ptr->s_flags	= ETH_SLOT_FREE;
		ins_slot_tail(&slot_free_list, s_ptr);
	}
}


/*===========================================================================*
 *                           get_slot_head	                                *
 * get the first slot from the head of a buffer ring			*
 *===========================================================================*/
slot_t *get_slot_head(buf_ring_t *b_ptr)
{
	slot_t *s_ptr;
	slot_t *h_ptr;
	
	_mhdebug("line",__LINE__);

	h_ptr = b_ptr->br_head;
	if( h_ptr->s_link.l_next == h_ptr ) /* point to itself */
		return(NULL);

	s_ptr = h_ptr->s_link.l_next;
	h_ptr->s_link.l_next = s_ptr->s_link.l_next;
	s_ptr->s_link.l_next->s_link.l_prev = h_ptr;
	
	b_ptr->br_used--;
	return(s_ptr);
}

/*===========================================================================*
 *                           rmv_slot	                                *
 *===========================================================================*/
slot_t *rmv_slot(buf_ring_t *br_ptr, slot_t *s_ptr)
{
	
	_mhdebug("line",__LINE__);

	s_ptr->s_link.l_prev->s_link.l_next = s_ptr->s_link.l_next;
	s_ptr->s_link.l_next->s_link.l_prev = s_ptr->s_link.l_prev;
	
	br_ptr->br_used--;
	return(s_ptr);
}

/*===========================================================================*
 *                           alloc_slot                                 *
 * take a free slot from the free list					*
 *===========================================================================*/
slot_t *alloc_slot(void)
{
	_mhdebug("line",__LINE__);

	return( get_slot_head(&slot_free_list));
}

/*===========================================================================*
 *                            free_frame                                    *
 * Could be replaced with free()					     *
 *===========================================================================*/
void free_frame(frame_t *f_ptr)
{
	_mhdebug("line",__LINE__);

	f_ptr->f_next = frame_free_list;  
	frame_free_list = f_ptr;
	f_ptr->f_count = 0;
	memset(&f_ptr->f_data, 0, ETH_FRAME_LEN); 
}

/*===========================================================================*
 *                            free_slot                                     *
 *===========================================================================*/
void free_slot( slot_t *sptr)
{
	int rcode;
	frame_t *f_ptr; 
	slot_t *s_ptr;
	
	_mhdebug("line",__LINE__);

	s_ptr->s_len	= 0;
	s_ptr->s_flags	= ETH_SLOT_FREE;
	f_ptr = s_ptr->s_faddr;
	if( --f_ptr->f_count == 0)
		free_frame(f_ptr);
	ins_slot_tail(&slot_free_list, s_ptr);
}

/*===========================================================================*
 *                            init_frames                                    *
 *===========================================================================*/
void init_frames(void)
{
	int i;

	_mhdebug("line",__LINE__);

	for( i=0; i < NR_ETH_FRAMES-1; i++){
		eth_frame[i].f_count = 0;
		memset(&eth_frame[i].f_data, 0, ETH_FRAME_LEN);
		eth_frame[i].f_next = &eth_frame[i+1];
	}
	eth_frame[i].f_next = NULL;
	eth_frame[i].f_count = 0;
	frame_free_list = &eth_frame[0];
}

/*===========================================================================*
 *                            alloc_frame                                    *
 * Could be replaced with malloc()					     *
 *===========================================================================*/
frame_t *alloc_frame(void)
{
	frame_t *f_ptr;

	_mhdebug("line",__LINE__);

	if( frame_free_list == NULL) return(NULL);
	f_ptr = frame_free_list;  
	frame_free_list = f_ptr->f_next;
	return(f_ptr);
}

/*****************************************************************************
 *                            mhved_task                                     *
 * MHDEB task must be executed as a PROMISCOUS DRIVER 	*
 ******************************************************************************/
void main( int argc, char **argv )
{
	message m, *m_ptr;	
  	int i,irq,r, inet_nr, vmm_nr;
 	ether_card_t *ec;
  	long v;

	int mhved_vmid;
	unsigned int new_vm = 0;
	
	MHDEBUG("mhved: argc=%d\n", argc);

#if MHDBG
	eth_tasknr = 0x1234;
#else
	if((eth_tasknr=getprocnr()) < 0)
		panic("mhved","couldn't get own proc nr", i);
#endif

	mhved_vmid = _mhdebug("MHVED: eth_tasknr", eth_tasknr);
	mhved_vmid = _mhdebug("MHVED2: vmid", mhved_vmid);
	vm_bitmap = (1 << mhved_vmid);

	init_frames();
	init_slots();
	init_cards();
	
	(progname=strrchr(argv[0],'/')) ? progname++ : (progname=argv[0]);

	env_setargs( argc, argv );
	
	v= 0;
 	(void) env_parse("ETH_IGN_PROTO", "x", 0, &v, 0x0000L, 0xFFFFL);
  	eth_ign_proto= htons((u16_t) v);

  	/* Try to notify inet that we are present (again) */
  	r = _pm_findproc("inet", &inet_nr);
	_mhdebug("MHVED inet_nr",inet_nr);
	
  	r = _pm_findproc("vmm", &vmm_nr);
	_mhdebug("MHVED vmm_nr",vmm_nr);
	
	notify(inet_nr);

	m_ptr =  &m;
	
  	while (TRUE)	{
		/* Get a request message. */
		rqst_vmid= recvvm(ANY, &m, ANY_VM);

		MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(m_ptr) );

		if (rqst_vmid < VM0) {
			_mhdebug("MHVED recvvm rqst_vmid", rqst_vmid);
			panic("MHVED", "recvvm failed with %d", rqst_vmid);
		}
		if (rqst_vmid >= VM0) {
			_mhdebug("MHVED rqst_vmid",rqst_vmid);
			_mhdebug("MHVED m.m_type",m.m_type);
			prtmsg(m_ptr->m_source, m_ptr);		
		}
		/*Verify if the request came from a registered VM */
		if( !TEST_BIT(vm_bitmap,rqst_vmid)){
			_mhdebug("MHVED vm_bitmap",vm_bitmap);
			_mhdebug("MHVED INVALID request from",rqst_vmid);
			continue;
		}
		/* Receive a notification from VMM informing a VM bitmap */
		if( (m.m_source == vmm_nr ) 
		&& (m.m_type == NOTIFY_FROM(vmm_nr))){
			m_ptr =  &m;
			_mhdebug("MHVED m.m_type",m.m_type);
			_mhdebug("MHVED m.NOTIFY_VMBITMAP",m.NOTIFY_VMBITMAP);
			_mhdebug("MHVED vm_bitmap",vm_bitmap);
			new_vm = ~vm_bitmap & m.NOTIFY_VMBITMAP;
			if( new_vm) {
				for( v = 0; v < NR_VMS ; v++){
					if( new_vm == (1 << v) ) {
						new_vm = v;
						_mhdebug("MHVED new_vm",new_vm);
						vm_bitmap |= (1 << new_vm);
						notifyvm(inet_nr, new_vm);
						break;
					}
				}
			}
			continue;
		}
        

      	switch (m.m_type){
      		case DEV_PING:   notifyvm(m.m_source,rqst_vmid);	continue;
      		case DL_WRITE:   do_vwrite(&m, FALSE);   		break;
      		case DL_WRITEV:  do_vwrite(&m, TRUE);    		break;
      		case DL_READ:    do_vread(&m, FALSE);           break;
      		case DL_READV:   do_vread(&m, TRUE);            break;
      		case DL_INIT:    do_init(&m);                   break;
      		case DL_GETSTAT: do_getstat(&m);                break;
      		case DL_STOP:    do_stop(&m);                   break;
      		case DL_GETNAME: do_getname(&m); 		break;
      		case FKEY_PRESSED: mhved_dump();                break;
      		/*case HARD_STOP:  mhved_stop();                break;*/
      		case SYS_SIG: {
      			sigset_t set = m.NOTIFY_ARG;
      			if ( sigismember( &set, SIGKSTOP ) )
      				mhved_stop();
		      	break;
			}
      		case HARD_INT:
			_mhdebug("HARD_INT from %d\n",m.m_source);
        		break;
      		case PROC_EVENT:
				break;
      		default:
        		panic( "mhved", "illegal message", m.m_type);
      	}

	if( 	(m.m_type == DL_WRITE)  || (m.m_type == DL_WRITEV) ||
      		(m.m_type == DL_READ)	|| (m.m_type == DL_READV) ) 
		/* RUN the frame switching function */
		vnet_switch();		
	}	
}

/*===========================================================================*
 *                            mhved_dump                                     *
 *===========================================================================*/
static void mhved_dump()
{
  	ether_card_t *ec;
  	int i, isr;
  	unsigned short ioaddr;
  
	_mhdebug("line",__LINE__);

  	MHDEBUG("\n");
  	for (i= 0, ec = &ec_table[0][rqst_vmid]; i<EC_PORT_NR_MAX; i++, ec++)    {
      		if (ec->mode == EC_DISABLED)
			MHDEBUG("mhved port %d is disabled\n", i);
      		else if (ec->mode == EC_SINK)
			MHDEBUG("mhved port %d is in sink mode\n", i);
      
      		if (ec->mode != EC_ENABLED)
			continue;
      
      		MHDEBUG("mhved statistics of port %d:\n", i);
      
      		MHDEBUG("recvErr    :%8ld\t", ec->eth_stat.ets_recvErr);
      		MHDEBUG("sendErr    :%8ld\t", ec->eth_stat.ets_sendErr);
      		MHDEBUG("OVW        :%8ld\n", ec->eth_stat.ets_OVW);
      
      		MHDEBUG("CRCerr     :%8ld\t", ec->eth_stat.ets_CRCerr);
      		MHDEBUG("frameAll   :%8ld\t", ec->eth_stat.ets_frameAll);
     		MHDEBUG("missedP    :%8ld\n", ec->eth_stat.ets_missedP);
      
      		MHDEBUG("packetR    :%8ld\t", ec->eth_stat.ets_packetR);
      		MHDEBUG("packetT    :%8ld\t", ec->eth_stat.ets_packetT);
      		MHDEBUG("transDef   :%8ld\n", ec->eth_stat.ets_transDef);
      
      		MHDEBUG("collision  :%8ld\t", ec->eth_stat.ets_collision);
      		MHDEBUG("transAb    :%8ld\t", ec->eth_stat.ets_transAb);
      		MHDEBUG("carrSense  :%8ld\n", ec->eth_stat.ets_carrSense);
      
      		MHDEBUG("fifoUnder  :%8ld\t", ec->eth_stat.ets_fifoUnder);
      		MHDEBUG("fifoOver   :%8ld\t", ec->eth_stat.ets_fifoOver);
      		MHDEBUG("CDheartbeat:%8ld\n", ec->eth_stat.ets_CDheartbeat);
      
      		MHDEBUG("OWC        :%8ld\t", ec->eth_stat.ets_OWC);
      
    	}
}

/*===========================================================================*
 *                              					                             *
 * Copy data from interface TX buffer to user space address		     *
 * THIS IS A VIRTUAL NETWORK SWITCH					     *
 * This algorithm is O(N^2)						     *
 * Its checks for frames from TX rings of all interfaces to RX rings of all  *
 * interfaces								     *	
 * Discard interfaces not enabled, VMs not enabled and RX,TX of the same card*			
 *===========================================================================*/
static void  vnet_switch(void )
{
	int v, j, vv, jj ,iface_up, frame_sent;
	ether_card_t *ect_ptr, *ecr_ptr;
	buf_ring_t *tx_ptr, *rx_ptr;
	slot_t *s_ptr, *tmp_ptr;
	frame_t * f_ptr;
	
	_mhdebug("line",__LINE__);

	/*.................... OUTER TX LOOP ..................................*/
	for( v=0; v < NR_VMS; v++){		
		if( !TEST_BIT(vm_bitmap, v))
			continue;
		for( j=0; j < EC_PORT_NR_MAX; j++){
			ect_ptr = &ec_table[j][v];
			if( !TEST_BIT(ect_ptr->flags, ECF_ENABLED))
				continue;

			tx_ptr=  &ect_ptr->ec_tx;
			s_ptr =  tx_ptr->br_head;

			while( s_ptr != NULL) {
				iface_up = FALSE;
				frame_sent = FRM_NOT_SENT;
				/*.................... INNER RX LOOP .................	*/
				/* searches all interfaces for a MAC address match	*/	
				for( vv=0; vv < NR_VMS; vv++){		
					if( !TEST_BIT(vm_bitmap, vv))
						continue;
					for( jj=0; jj < EC_PORT_NR_MAX; jj++){
						/* if( v == vv && j =jj) continue; */
						ecr_ptr = &ec_table[jj][vv];
						if( ! TEST_BIT(ecr_ptr->flags, ECF_ENABLED))
							continue;
						iface_up = TRUE;
						f_ptr =  s_ptr->s_faddr;
						/*check if the DST MAC address of the frame 	*/
						/* matches the interface MAC address		*/
						/* or it is a multicast or broadcast		*/
						if( memcmp( (void *) &f_ptr->f_data.f_eth_hdr.eh_dst, 
								(void *)&ecr_ptr->mac_address, 
								MAC_ADDR_LEN) == 0) {
							frame_tx2rx(s_ptr, rx_ptr, FRM_SENT_UCAST);
							frame_sent = FRM_SENT_UCAST;
						}else if (memcmp( (void *) &f_ptr->f_data.f_eth_hdr.eh_dst, 
								(void *) &mac_addr_bcast , 
								MAC_ADDR_LEN) == 0) {
							frame_tx2rx(s_ptr,rx_ptr, FRM_SENT_BCAST);
							frame_sent = FRM_SENT_BCAST;
						}else if( f_ptr->f_data.f_eth_hdr.eh_dst.ea_addr[0] & MCAST_MASK) {
							frame_tx2rx(s_ptr,rx_ptr, FRM_SENT_BCAST);
							frame_sent = FRM_SENT_MCAST;
						}
						if( frame_sent == FRM_SENT_UCAST) break;
					}
					if( frame_sent == FRM_SENT_UCAST) break;
				}
				/*.................... END RX LOOP ....................*/
				if( !iface_up || frame_sent ) {
					/* remove the slot from the ring*/
					/* and get the next slot	*/
					tmp_ptr = rmv_slot(tx_ptr, s_ptr);
					/* discard the slot */
					free_slot(s_ptr);
					s_ptr = tmp_ptr;
				} else {
					s_ptr = s_ptr->s_link.l_next;
				}
			}			
		}
	}
	/*.................... END TX LOOP ..................................*/
}


/*===========================================================================*
 *                               mhved_stop                                  *
 *===========================================================================*/
static void mhved_stop()
{
  	message mess;
  	int i;
	
	_mhdebug("line",__LINE__);

  	for (i= 0; i<EC_PORT_NR_MAX; i++) {
      		if (ec_table[i][rqst_vmid].mode != EC_ENABLED)
			continue;
      		mess.m_type= DL_STOP;
      		mess.DL_PORT= i;
      		do_stop(&mess);
    	}
    
  	MHDEBUG("MHVED driver stopped.\n");
  	
  	sys_exit( 0 );
}

/*===========================================================================*
 *                              do_vwrite                                    *
 *===========================================================================*/
static void do_vwrite(mp,vectored)
message *mp;
int vectored;
{
	int port, count, check;
	ether_card_t *ec;
	unsigned short ioaddr;
	message reply_mess;

	MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(mp) );

	port = mp->DL_PORT;
  	if (port < 0 || port >= EC_PORT_NR_MAX)	{
      		reply_mess.m_type= DL_INIT_REPLY;
      		reply_mess.m3_i1= ENXIO;
      		mess_reply(mp, &reply_mess);
      		return;
    	}
	count = mp->DL_COUNT;
	ec = &ec_table[port][rqst_vmid];
	ec->client= mp->DL_PROC;

	/* convert the message to write_iovec */
	if (vectored) {
		get_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
                   (count > IOVEC_NR ? IOVEC_NR : count) *
                   sizeof(iovec_t), ec->write_iovec.iod_iovec);

		ec->write_iovec.iod_iovec_s    = count;
		ec->write_iovec.iod_proc_nr    = mp->DL_PROC;
		ec->write_iovec.iod_iovec_addr = (vir_bytes) mp->DL_ADDR;

		ec->tmp_iovec = ec->write_iovec;
		ec->write_s = calc_iovec_size(&ec->tmp_iovec);
    	} else  {  
		ec->write_iovec.iod_iovec[0].iov_addr = (vir_bytes) mp->DL_ADDR;
		ec->write_iovec.iod_iovec[0].iov_size = mp->DL_COUNT;

		ec->write_iovec.iod_iovec_s    = 1;
		ec->write_iovec.iod_proc_nr    = mp->DL_PROC;
		ec->write_iovec.iod_iovec_addr = 0;

		ec->write_s = mp->DL_COUNT;
    	}

       	ec->write_iovec.iod_index = 0;
	ec->flags |= ECF_PACK_SEND;

	/* do not reply to the client*/
	reply(ec, OK, FALSE);
}

/*===========================================================================*
 *                              do_stop                                      *
 *===========================================================================*/
static void do_stop(mp)
message *mp;
{
  	int port;
  	ether_card_t *ec;

	MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(mp) );

  	port = mp->DL_PORT;
  	ec = &ec_table[port][rqst_vmid];

  	if (!(ec->flags & ECF_ENABLED))
    		return;

	ec->flags = ECF_EMPTY;
}

/*===========================================================================*
 *                              do_getstat                                   *
 *===========================================================================*/
static void do_getstat(mp)
message *mp;
{
  	int port;
  	ether_card_t *ec;

	MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(mp) );

  	port = mp->DL_PORT;
  	ec= &ec_table[port][rqst_vmid];
  	ec->client= mp->DL_PROC;

  	put_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
               (vir_bytes) sizeof(ec->eth_stat), &ec->eth_stat);
 	reply(ec, OK, FALSE);
}

/*===========================================================================*
 *				do_getname				     *
 *===========================================================================*/
static void do_getname(mp)
message *mp;
{
	int r;

	MHDEBUG(MSG3_FORMAT, MSG3_FIELDS(mp) );

	strncpy(mp->DL_NAME, progname, sizeof(mp->DL_NAME));
	mp->DL_NAME[sizeof(mp->DL_NAME)-1]= '\0';
	mp->m_type= DL_NAME_REPLY;
	r= send(mp->m_source, mp);
}
/*===========================================================================*
 *                              do_vread                                     *
 *===========================================================================*/
static void do_vread(mp, vectored)
message *mp;
int vectored;
{
  	int port, count, size;
  	ether_card_t *ec;
	message reply_mess;

	MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(mp) );

  	count = mp->DL_COUNT;

  	port = mp->DL_PORT;
  	if (port < 0 || port >= EC_PORT_NR_MAX)	{
      		reply_mess.m_type= DL_INIT_REPLY;
      		reply_mess.m3_i1= ENXIO;
      		mess_reply(mp, &reply_mess);
      		return;
    	}

  	ec= &ec_table[port][rqst_vmid];
  	ec->client= mp->DL_PROC;

  	if (vectored)    {
      		get_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
                   (count > IOVEC_NR ? IOVEC_NR : count) *
                   sizeof(iovec_t), ec->read_iovec.iod_iovec);
      		ec->read_iovec.iod_iovec_s    = count;
      		ec->read_iovec.iod_proc_nr    = mp->DL_PROC;
      		ec->read_iovec.iod_iovec_addr = (vir_bytes) mp->DL_ADDR;
      
      		ec->tmp_iovec = ec->read_iovec;
      		size= calc_iovec_size(&ec->tmp_iovec);
    	} else {
      		ec->read_iovec.iod_iovec[0].iov_addr = (vir_bytes) mp->DL_ADDR;
      		ec->read_iovec.iod_iovec[0].iov_size = mp->DL_COUNT;
      		ec->read_iovec.iod_iovec_s           = 1;
      		ec->read_iovec.iod_proc_nr           = mp->DL_PROC;
      		ec->read_iovec.iod_iovec_addr        = 0;
		size= count;
    	}
 
	ec->flags |= ECF_READING;

  	ec_recv(ec);

  	if ((ec->flags & (ECF_READING|ECF_STOPPED)) == (ECF_READING|ECF_STOPPED))
    		ec_reset(ec);
  	reply(ec, OK, FALSE);
}

/*===========================================================================*
 *                              do_init                                      *
 *===========================================================================*/
static void do_init(mp)
message *mp;
{
  	int port;
  	ether_card_t *ec;
 	message reply_mess;

	MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(mp) );

	port = mp->DL_PORT;
  	if (port < 0 || port >= EC_PORT_NR_MAX)	{
      		reply_mess.m_type= DL_INIT_REPLY;
      		reply_mess.m3_i1= ENXIO;
      		mess_reply(mp, &reply_mess);
      		return;
    	}
  	ec= &ec_table[port][rqst_vmid];
 

  	if (ec->mode == EC_DISABLED){
	      	ec->mode = EC_ENABLED;
		ec_init(ec);
    	} else if (ec->mode == EC_SINK) {
		memset(&ec->mac_address,0,MAC_ADDR_LEN);
    		ec_confaddr(ec);
		reply_mess.m_type = DL_INIT_REPLY;
      		reply_mess.m3_i1 = mp->DL_PORT;
      		reply_mess.m3_i2 = EC_PORT_NR_MAX;
      		*(ether_addr_t *) reply_mess.m3_ca1 = ec->mac_address;
      		mess_reply(mp, &reply_mess);
      		return;
    	}

  	ec->flags &= ~(ECF_PROMISC | ECF_MULTI | ECF_BROAD);

  	if (mp->DL_MODE & DL_PROMISC_REQ)
    		ec->flags |= ECF_PROMISC | ECF_MULTI | ECF_BROAD;
  	if (mp->DL_MODE & DL_MULTI_REQ)
    		ec->flags |= ECF_MULTI;
  	if (mp->DL_MODE & DL_BROAD_REQ)
    		ec->flags |= ECF_BROAD;

  	ec->client = mp->m_source;
  	ec_reinit(ec);

  	reply_mess.m_type = DL_INIT_REPLY;
  	reply_mess.m3_i1 = mp->DL_PORT;
  	reply_mess.m3_i2 = EC_PORT_NR_MAX;
  	*(ether_addr_t *) reply_mess.m3_ca1 = ec->mac_address;

  	mess_reply(mp, &reply_mess);
}

/*===========================================================================*
 *                             frame_tx2rx	                             *
 * Copy a frame from TX interface to RX interface 			     *
 *===========================================================================*/
static void frame_tx2rx(slot_t *s_ptr, buf_ring_t *rx_ptr, int frm_type)
{
	slot_t *d_ptr;
	
	d_ptr = alloc_slot();
	if(d_ptr == NULL) return;
	
	d_ptr->s_len = s_ptr->s_len;
	if( frm_type == FRM_SENT_UCAST)
		d_ptr->s_flags =  ETH_SLOT_RX_RING;
	else
		d_ptr->s_flags =  ETH_SLOT_RX_MCAST;
	
	d_ptr->s_faddr = s_ptr->s_faddr;
	s_ptr->s_faddr->f_count++;
	ins_slot_tail(rx_ptr, d_ptr);

}

/*===========================================================================*
 *                              put_userdata                                 *
 *===========================================================================*/
static void put_userdata(user_proc, user_addr, count, loc_addr)
int user_proc;
vir_bytes user_addr;
vir_bytes count;
void *loc_addr;
{
	int cps;
	cps = sys_datacopy(SELF, (vir_bytes) loc_addr, user_proc, user_addr, count);
	if (cps != OK) MHDEBUG("mhved: warning, scopy failed: %d\n", cps);
}

/*===========================================================================*
 *                              ec_reset                                     *
 *===========================================================================*/
static void ec_reset(ec)
ether_card_t *ec;
{
	/* purge Tx-ring */
  	ec->flags &= ~ECF_STOPPED;
}

/*===========================================================================*
 *                              get_userdata                                 *
 *===========================================================================*/
static void get_userdata(user_proc, user_addr, count, loc_addr)
int user_proc;
vir_bytes user_addr;
vir_bytes count;
void *loc_addr;
{
	int cps;
	cps = sys_datacopy(user_proc, user_addr, SELF, (vir_bytes) loc_addr, count);
	if (cps != OK) MHDEBUG("mhved: warning, scopy failed: %d\n", cps);
}

/*===========================================================================*
 *                              calc_iovec_size                              *
 *===========================================================================*/
static int calc_iovec_size(iovp)
iovec_dat_t *iovp;
{
	int size,i;

	size = i = 0;
        
	while (i < iovp->iod_iovec_s)  {
		if (i >= IOVEC_NR) {
			ec_next_iovec(iovp);
			i= 0;
			continue;
		}
		size += iovp->iod_iovec[i].iov_size;
		i++;
    }

  return size;
}

/*===========================================================================*
 *                              mess_reply                                   *
 *===========================================================================*/
static void mess_reply(req, reply_mess)
message *req;
message *reply_mess;
{
	MHDEBUG(MSG2_FORMAT, MSG2_FIELDS(reply_mess) );

  	if (send(req->m_source, reply_mess) != OK)
    		panic( "mhved", "unable to mess_reply", NO_NUM);
}

/*===========================================================================*
 *                              reply                                        *
 *===========================================================================*/
static void reply(ec, err, may_block)
ether_card_t *ec;
int err;
int may_block;
{
  	message reply;
  	int status,r;
  	clock_t now;

  	status = 0;
  	if (ec->flags & ECF_PACK_SEND)
    		status |= DL_PACK_SEND;
 	if (ec->flags & ECF_PACK_RECV)
    		status |= DL_PACK_RECV;

  	reply.m_type   = DL_TASK_REPLY;
  	reply.DL_PORT  = ec->ec_port;
  	reply.DL_PROC  = ec->client;
  	reply.DL_STAT  = status | ((u32_t) err << 16);
  	reply.DL_COUNT = ec->read_s;
	if ((r=getuptime(&now)) != OK)
		panic("mhved", "getuptime() failed:", r);
  	reply.DL_CLCK = now; 
	r = send(ec->client, &reply);
	if (r == ELOCKED && may_block)
	      return;
	if (r < 0)
    		panic( "mhved", "send failed:", r);

  	ec->read_s = 0;
  	ec->flags &= ~(ECF_PACK_SEND | ECF_PACK_RECV);
}

/*===========================================================================*
 *                              ec_init                                      *
 *===========================================================================*/
static void ec_init(ec)
ether_card_t *ec;
{
  	int i, r;

  	/* General initialization */
  	ec->flags = ECF_EMPTY;

  	mhved_init_card(ec); /* Get mac_address, etc ...*/

  	ec_confaddr(ec);

  	/* Finish the initialization */
  	ec->flags |= ECF_ENABLED;

  return;
}

/*===========================================================================*
 *                            mhved_init_card                                *
 *===========================================================================*/
static void mhved_init_card(ec)
ether_card_t *ec;
{
	 ec_confaddr(ec); 
}

/*===========================================================================*
 *                              ec_reinit                                    *
 *===========================================================================*/
static void ec_reinit(ec)
ether_card_t *ec;
{

  return;
}

/*===========================================================================*
 *                              ec_recv                                      *
 *===========================================================================*/
static void ec_recv(ec)
ether_card_t *ec;
{

}

/*===========================================================================*
 *                           ec_next_iovec                                   *
 *===========================================================================*/
static void ec_next_iovec(iovp)
iovec_dat_t *iovp;
{
  iovp->iod_iovec_s -= IOVEC_NR;
  iovp->iod_iovec_addr += IOVEC_NR * sizeof(iovec_t);

  get_userdata(iovp->iod_proc_nr, iovp->iod_iovec_addr, 
               (iovp->iod_iovec_s > IOVEC_NR ? 
                IOVEC_NR : iovp->iod_iovec_s) * sizeof(iovec_t), 
               iovp->iod_iovec); 
}

/*===========================================================================*
 *                              ec_confaddr                                  *
 *===========================================================================*/
static void ec_confaddr(ec)
ether_card_t *ec;
{
	int i;
	_mhdebug("%s:%d MAC address ", ec->port_name, ec->ec_vmid);
	for (i = 0; i < MAC_ADDR_LEN; i++){
		if( i < (MAC_ADDR_LEN-1)){
			ec->mac_address.ea_addr[i]= mac_addr_base.ea_addr[i];
			_mhdebug("%x:", ec->mac_address.ea_addr[i]);
		}else{
			ec->mac_address.ea_addr[i]= mac_addr_base.ea_addr[i] + ec->ec_vmid;
			_mhdebug("%x\n", ec->mac_address.ea_addr[i]);
		}
	}		
}


#if ANULADO
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/

/*===========================================================================*
 *                              do_int                                       *
 *===========================================================================*/
static void do_int(ec)
ether_card_t *ec;
{
  	if (ec->flags & (ECF_PACK_SEND | ECF_PACK_RECV))
    		reply(ec, OK, TRUE);
}


/*===========================================================================*
 *                              conf_hw                                      *
 *===========================================================================*/
static void conf_hw(ec)
ether_card_t *ec;
{
	static eth_stat_t empty_stat = {0, 0, 0, 0, 0, 0        /* ,... */ };
	ec_conf_t *ecp;

	ec->mode = EC_DISABLED;     /* Superfluous */
	ecp= &ec_conf[ec->ec_port];
	update_conf(ec, ecp);
	if (ec->mode != EC_ENABLED)
		return;

	ec->flags = ECF_EMPTY;
	ec->eth_stat = empty_stat;
}


/*===========================================================================*
 *                              update_conf                                  *
 *===========================================================================*/
static void update_conf(ec, ecp)
ether_card_t *ec;
ec_conf_t *ecp;
{
  	long v;
  	static char ec_fmt[] = "x:d:x:x";

  	/* Get the default settings and modify them from the environment. */
  	ec->mode= EC_SINK;
 	v= ec->ec_port;
  	switch (env_parse(ecp->ec_envvar, ec_fmt, 0, &v, 0x0000L, 0xFFFFL)) {
  		case EP_OFF:
    			ec->mode= EC_DISABLED;
    			break;
  		case EP_ON:
  		case EP_SET:
    		ec->mode= EC_ENABLED;      /* Might become disabled if 
					* all probes fail */
    		break;
  	}
  	ec->ec_port= v;
}





/*===========================================================================*
 *                              ec_send                                      *
 *===========================================================================*/
static void ec_send(ec)
ether_card_t *ec;
{

  	switch(ec->sendmsg.m_type)  {
    		case DL_WRITE:  
			do_vwrite(&ec->sendmsg, FALSE);       
			break;
    		case DL_WRITEV: 
			do_vwrite(&ec->sendmsg, TRUE);        
			break;
    		default:
      			panic( "mhved", "wrong type:", ec->sendmsg.m_type);
      			break;
    	}
}




/*===========================================================================*
 *                              ec_usr2tx				*
 * Copy data from user space address to the interface TX buffer		*
 * ec->write_iovec.iod_index: from which vector element start the copy? *
 * ec->write_iovec.iod_iovec_s: how many elements the vector have? 	*
 *===========================================================================*/
static void ec_usr2rx(ether_card_t *ec)
{
	slot_t *s_ptr;
	frame_t *f_ptr;

	int i;

	while (ec->write_iovec.iod_iovec_s > 0) {

		/* verify the copy size . It must be less o equal to Ethernet Frame lenght */
		i = ec->write_iovec.iod_index;
	
		/* verify TX ring space and the availability of free slots */
		if( (	ec->ec_tx.br_free == 0) || 		/* can I insert the slot into the TX ring ? */
			(slot_free_list.br_free == 0) ){	/* can I  get a free slot? */
			SET_BIT(ec->flags, ECF_PACK_SEND);
			return;
		}

		s_ptr = alloc_slot();

		/* get a free frame */
		s_ptr->s_faddr = alloc_frame();

		if ( (r=sys_datacopy( 
			ec->write_iovec.iovp.iod_proc_nr, 
			ec->write_iovec.iovp.iod_iovec[i].iov_addr, 
			SELF, &s_ptr->s_faddr->f_data, 
			ec->write_iovec.iovp.iod_iovec[i].iov_size )) != OK )
			panic( "mhved", "sys_datacopy failed", r );
      	
		/* compare SOURCE MAC addresses */
		if( memcmp(&ec->mac_address, &s_ptr->f_addr->f_payload.p_eth_hdr.eh_src,MAC_ADDR_LEN)!=0 ) {
			/* discard the frame */
			free_frame(s_ptr->s_faddr);
			free_slot(s_ptr);
			_mhdebug("ec_usr2tx: frame discarded by port=%s\n", ec->port_name);
		} else {
			s_ptr->s_len = ec->write_iovec.iovp.iod_iovec[i].iov_size;
			s_ptr->s_flags = ETH_SLOT_TX_RING;
			s_ptr->s_faddr->f_count = 1;

			/* insert the slot into the tx ring */
			ins_slot_tail(&ec->ec_tx, s_ptr);
		}

		ec->write_iovec.iod_index++;
		ec->write_iovec.iod_iovec_s--;

    	}
	CLR_BIT(ec->flags, ECF_PACK_SEND);
}

/*===========================================================================*
 *                              ec_rx2usr	                             *
 * Copy data from interface RX buffer to user space address		     *
 * ec->read_iovec.iod_index: from which vector element start the copy? *
 * ec->read_iovec.iod_iovec_s: how many elements the vector have? 	*
 *===========================================================================*/
static void ec_rx2usr(ether_card_t *ec)
{
	slot_t *s_ptr;
	int i , count;


	while (ec->read_iovec.iod_iovec_s > 0) {

		/* get a slot from RX buffer */
		s_ptr = get_slot_head(&ec->ec_rx);

		/* the RX buffer is EMPTY */
		if( s_ptr == NULL){
			SET_BIT(ec->flags, ECF_PACK_RECV);
			return;
		}

		i = ec->read_iovec.iod_index;

		/*transfer the lower bytes count between the frame and the user request */
		count = (ec->read_iovec.iovp.iod_iovec[i].iov_size < s_ptr->s_len) ?
			ec->read_iovec.iovp.iod_iovec[i].iov_size :
			s_ptr->s_len;

		if ( (r=sys_datacopy( SELF, s_ptr->s_faddr->fdata, 
			ec->read_iovec.iovp.iod_proc_nr, 
			ec->read_iovec.iovp.iod_iovec[i].iov_addr, 
			count )) != OK )
			panic( "mhved", "sys_datacopy failed", r ); 	


		free_frame(s_ptr->s_faddr);
		free_slot(s_ptr);

		ec->read_iovec.iod_index++;
		ec->read_iovec.iod_iovec_s--;
    }
	/* the RX buffer is EMPTY */
	if( s_ptr == NULL){
		CLR_BIT(ec->flags, ECF_PACK_RECV);
		return;
	}
}




/*===========================================================================*
 *                            mhved_probe                                    *
 *===========================================================================*/
static int mhved_probe(ec)
ether_card_t *ec;
{
	int   mhved_version = 0x1234;
	return mhved_version;
}




#endif /* ANULADO */
