#ifndef _IPC_H
#define _IPC_H

/*==========================================================================* 
 * Types relating to messages. 						    *
 *==========================================================================*/ 

#define M1                 1
#define M3                 3
#define M4                 4
#define M3_STRING         14

typedef struct {int m1i1, m1i2, m1i3; char *m1p1, *m1p2, *m1p3;} mess_1;
typedef struct {int m2i1, m2i2, m2i3; long m2l1, m2l2; char *m2p1;} mess_2;
typedef struct {int m3i1, m3i2; char *m3p1; char m3ca1[M3_STRING];} mess_3;
typedef struct {long m4l1, m4l2, m4l3, m4l4, m4l5;} mess_4;
typedef struct {short m5c1, m5c2; int m5i1, m5i2; long m5l1, m5l2, m5l3;}mess_5;
typedef struct {int m7i1, m7i2, m7i3, m7i4; char *m7p1, *m7p2;} mess_7;
typedef struct {int m8i1, m8i2; char *m8p1, *m8p2, *m8p3, *m8p4;} mess_8;

typedef struct {
  int m_source;			/* who sent the message */
  int m_type;			/* what kind of message is it */
  union {
	mess_1 m_m1;
	mess_2 m_m2;
	mess_3 m_m3;
	mess_4 m_m4;
	mess_5 m_m5;
	mess_7 m_m7;
	mess_8 m_m8;
  } m_u;
} message;

#ifdef MHYPER
#define MSG1_FORMAT "source=%d type=%d m1i1=%d m1i2=%d m1i3=%d m1p1=%p m1p2=%p m1p3=%p \n"
#define MSG2_FORMAT "source=%d type=%d m2i1=%d m2i2=%d m2i3=%d m2l1=%d m2l2=%d m2p1=%p\n"
#define MSG3_FORMAT "source=%d type=%d m3i1=%d m3i2=%d m3p1=%p m3ca1=[%s]\n"
#define MSG4_FORMAT "source=%d type=%d m4l1=%d m4l2=%d m4l3=%d m4l4=%d m4l5=%d\n"
#define MSG5_FORMAT "source=%d type=%d m5c1=%c m5c2=%c m5i1=%d m5i2=%d m5l1=%d m5l2=%d m5l3=%d\n"
#define MSG6_FORMAT "source=%d type=%d m6ca1=[%s]\n"
#define MSG7_FORMAT "source=%d type=%d m7i1=%d m7i2=%d m7i3=%d m7i4=%d m7p1=%p m7p2=%p\n"
#define MSG8_FORMAT "source=%d type=%d m8i1=%d m8i2=%d m8p1=%p m8p2=%p m8p3=%p m8p4=%p\n"

#define MSG1_FIELDS(p) 	p->m_source,p->m_type, p->m1_i1, p->m1_i2, p->m1_i3, p->m1_p1, p->m1_p2, p->m1_p3
#define MSG2_FIELDS(p) 	p->m_source,p->m_type, p->m2_i1, p->m2_i2, p->m2_i3, p->m2_l1, p->m2_l2, p->m2_p1
#define MSG3_FIELDS(p) 	p->m_source,p->m_type, p->m3_i1, p->m3_i2, p->m3_p1, p->m3_ca1
#define MSG4_FIELDS(p) 	p->m_source,p->m_type, p->m4_l1, p->m4_l2, p->m4_l3, p->m4_l4, p->m4_l5
#define MSG5_FIELDS(p) 	p->m_source,p->m_type, p->m5_c1, p->m5_c2, p->m5_i1, p->m5_i2, p->m5_l1, p->m5_l2, p->m5_l3
#define MSG6_FIELDS(p) 	p->m_source,p->m_type, p->m6_ca1
#define MSG7_FIELDS(p) 	p->m_source,p->m_type, p->m7_i1, p->m7_i2, p->m7_i3, p->m7_i4, p->m7_p1, p->m7p2
#define MSG8_FIELDS(p) 	p->m_source,p->m_type, p->m8_i1, p->m8_i2, p->m8_p1, p->m8_p2, p->m8_p3, p->m8_p4
#endif /*  MHYPER */

/* The following defines provide names for useful members. */
#define m1_i1  m_u.m_m1.m1i1
#define m1_i2  m_u.m_m1.m1i2
#define m1_i3  m_u.m_m1.m1i3
#define m1_p1  m_u.m_m1.m1p1
#define m1_p2  m_u.m_m1.m1p2
#define m1_p3  m_u.m_m1.m1p3

#define m2_i1  m_u.m_m2.m2i1
#define m2_i2  m_u.m_m2.m2i2
#define m2_i3  m_u.m_m2.m2i3
#define m2_l1  m_u.m_m2.m2l1
#define m2_l2  m_u.m_m2.m2l2
#define m2_p1  m_u.m_m2.m2p1

#define m3_i1  m_u.m_m3.m3i1
#define m3_i2  m_u.m_m3.m3i2
#define m3_p1  m_u.m_m3.m3p1
#define m3_ca1 m_u.m_m3.m3ca1

#define m4_l1  m_u.m_m4.m4l1
#define m4_l2  m_u.m_m4.m4l2
#define m4_l3  m_u.m_m4.m4l3
#define m4_l4  m_u.m_m4.m4l4
#define m4_l5  m_u.m_m4.m4l5

#define m5_c1  m_u.m_m5.m5c1
#define m5_c2  m_u.m_m5.m5c2
#define m5_i1  m_u.m_m5.m5i1
#define m5_i2  m_u.m_m5.m5i2
#define m5_l1  m_u.m_m5.m5l1
#define m5_l2  m_u.m_m5.m5l2
#define m5_l3  m_u.m_m5.m5l3

#define m7_i1  m_u.m_m7.m7i1
#define m7_i2  m_u.m_m7.m7i2
#define m7_i3  m_u.m_m7.m7i3
#define m7_i4  m_u.m_m7.m7i4
#define m7_p1  m_u.m_m7.m7p1
#define m7_p2  m_u.m_m7.m7p2

#define m8_i1  m_u.m_m8.m8i1
#define m8_i2  m_u.m_m8.m8i2
#define m8_p1  m_u.m_m8.m8p1
#define m8_p2  m_u.m_m8.m8p2
#define m8_p3  m_u.m_m8.m8p3
#define m8_p4  m_u.m_m8.m8p4

/*==========================================================================* 
 * Minix run-time system (IPC). 					    *
 *==========================================================================*/ 

/* Hide names to avoid name space pollution. */
#define echo		_echo
#define notify		_notify
#define sendrec		_sendrec
#define receive		_receive
#define send		_send

#ifdef MHYPER
#define hdebug		_hdebug
#define prtmsg		_prtmsg

#define sendas		_sendas
#define recvas		_recvas
#define relay		_relay
#define notifyas	_notifyas

#define sendvm		_sendvm
#define recvvm		_recvvm
#define sendrecvm	_sendrecvm
#define notifyvm	_notifyvm

#define nodebug		_nodebug


#endif /*  MHYPER */

_PROTOTYPE( int echo, (message *m_ptr)					);
_PROTOTYPE( int notify, (int dest)					);
_PROTOTYPE( int sendrec, (int src_dest, message *m_ptr)			);
_PROTOTYPE( int receive, (int src, message *m_ptr)			);
_PROTOTYPE( int send, (int dest, message *m_ptr)			);

#ifdef MHYPER
_PROTOTYPE( int hdebug,  (char *text, int code)			);
_PROTOTYPE( int prtmsg,  (int m_source, message *m_ptr)		);

_PROTOTYPE( int sendas,  (int dst_ep, message *m_ptr, int src_p));
_PROTOTYPE( int recvas, (int src_ep, message *m_ptr, int dst_p));
_PROTOTYPE( int relay, (int dst_ep, message *m_ptr, int src_ep));
_PROTOTYPE( int notifyas, (int src_p, int dst_p));


_PROTOTYPE( int sendvm,  (int dst_ep, message *m_ptr, int vmid));
_PROTOTYPE( int recvvm, (int src_ep, message *m_ptr, int vmid));
_PROTOTYPE( int sendrecvm, (int src_ep, message *m_ptr, int vmid));
_PROTOTYPE( int notifyvm, (int dest, int vmid)				);
_PROTOTYPE( int nodebug,  (char *text, int code)			);


#endif /*  MHYPER */

#define ipc_request	_ipc_request
#define ipc_reply	_ipc_reply
#define ipc_notify	_ipc_notify
#define ipc_select	_ipc_select

_PROTOTYPE( int ipc_request, (int dst, message *m_ptr)			);
_PROTOTYPE( int ipc_reply, (int dst, message *m_ptr)			);
_PROTOTYPE( int ipc_notify, (int dst, long event_set)			);
_PROTOTYPE( int ipc_receive, (int src, long events, message *m_ptr)	);


#endif /* _IPC_H */
