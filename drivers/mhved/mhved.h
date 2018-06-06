/*#include "kernel.h"*/
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>


/* supported max number of ether cards for each VM */
#define EC_PORT_NR_MAX 1

/* macros for 'mode' */
#define EC_DISABLED    0x0
#define EC_SINK        0x1
#define EC_ENABLED     0x2

/* macros for 'flags' */
#define ECF_EMPTY       0x000
#define ECF_PACK_SEND   0x001		/* Pending SEND/WRITES on this interface */
#define ECF_PACK_RECV   0x002
#define ECF_SEND_AVAIL  0x004
#define ECF_READING     0x010
#define ECF_PROMISC     0x040
#define ECF_MULTI       0x080
#define ECF_BROAD       0x100
#define ECF_ENABLED     0x200
#define ECF_STOPPED     0x400

/* === macros for ether cards (our generalized version) === */
#define EC_ISR_RINT     0x0001
#define EC_ISR_WINT     0x0002
#define EC_ISR_RERR     0x0010
#define EC_ISR_WERR     0x0020
#define EC_ISR_ERR      0x0040
#define EC_ISR_RST      0x0100

/* IOVEC */
#define IOVEC_NR        16
typedef struct iovec_dat
{
  iovec_t iod_iovec[IOVEC_NR];
  int iod_iovec_s;
  int iod_proc_nr;
  int iod_index;		/* next index in the iod_iovec to process */
  vir_bytes iod_iovec_addr;
} iovec_dat_t;


#define ETH_SLOT_FREE		0x00
#define	ETH_SLOT_RX_RING	0x01
#define ETH_SLOT_TX_RING	0x02
#define ETH_SLOT_RX_MCAST  	0x03
#define ETH_SLOT_HEAD 		0xFF

#define	FRM_NOT_SENT		0x00
#define	FRM_SENT_UCAST		0x01
#define	FRM_SENT_MCAST		0x02
#define	FRM_SENT_BCAST		0x03

#define ETH_MCAST_MASK		(1 << 7)
#define ETH_FRAME_LEN           1518
#define	NR_ETH_SLOTS		2*NR_VMS*16				/* Heuristics */
#define	NR_ETH_FRAMES		NR_VMS*NR_ETH_SLOTS		/* Heuristics */

#define MAC_ADDR_LEN 		sizeof(ether_addr_t) 

struct link_s {
	struct slot_s		*l_next;	/* next slot in the 	ring */
	struct slot_s		*l_prev;	/* previous slot in the ring*/
};
typedef struct link_s link_t;

struct frame_s {
	int  		f_count;	/* number of slots that point the frame */
	struct frame_s		*f_next;	/* next frame the	ring */
	struct		 {
		eth_hdr_t 	f_eth_hdr;
		char 		f_payload[ETH_FRAME_LEN - sizeof(eth_hdr_t)];
		}		f_data;
};
typedef struct frame_s frame_t;

struct slot_s	{
	int  		s_nr;		/* slot identification 		*/
	int  		s_len;		/* number of octets used 	*/
	unsigned int 	s_flags;	/* status of the slot 		*/
	link_t		s_link;		/* links to other slots in the ring	*/
	frame_t		*s_faddr;	/* frame address 		*/
	};
typedef struct slot_s slot_t;

struct buf_ring_s {
	int	br_used;		/* current number of slots  	*/
	slot_t	*br_head;		/* ring head 			*/
};
typedef struct buf_ring_s buf_ring_t;

#if MHDBG  
#define MHDEBUG printf
#else  /*  MHDBG */ 
#define MHDEBUG if(0) printf
#endif  /*MHDBG */

/* ====== ethernet card info. ====== */
typedef struct ether_card
{
  /* ####### MINIX style ####### */
  char port_name[sizeof("mhved_card#0")];
  int flags;
  int mode;
  int transfer_mode;
  eth_stat_t eth_stat;
  iovec_dat_t read_iovec;
  iovec_dat_t write_iovec;
  iovec_dat_t tmp_iovec;
  vir_bytes write_s;
  vir_bytes read_s;
  int client;
  message sendmsg;

  /* ######## device info. ####### */ 	

  port_t ec_port;
  int	ec_vmid;
  char *ec_envvar;

  buf_ring_t	ec_rx;		/* RX ring buffer head*/
  buf_ring_t 	ec_tx;		/* TX ring buffer head*/
	
  ether_addr_t mac_address;
} ether_card_t;

#define DEI_DEFAULT    0x8000

