/***************************************************************************

 				General MNP Definitions

	8/27/87 - RBUF_LEN increased to 576..gp
	10/29/87 - explict data typing..gp

***************************************************************************/

/* Data Types 
*/
#define SIGN_8 signed char
#define USIGN_8 unsigned char
#define SIGN_16 signed short int
#define USIGN_16 unsigned short int

#define SUCCESS 	0	/* success return code from all functions */
#define TRUE		1
#define FALSE		0

/*-------------------- Return Codes used by LLVL -------------------------*/

#define FAILURE      (-64)	/* no further information */
#define TIME_OUT     (-65)	/* timeout */
#define NO_PHYSICAL  (-66)	/* physical connection down */
#define LNK_DOWN     (-67)	/* link connection down */
				/* see mnp_cb.ld_reason for LD reason code */
#define LR_CODE      (-73)	/* when there is an error in parsing an LR,
				   an LD is sent back with reason code (1-5)
				   LR_CODE is added to that reason code and
				   returned as one of the 5 error codes below

		      -72	received LPDU not LR as expected
		      -71	reserved
		      -70	incompatible or unknown parameters
		      -69	reserved
		      -68	reserved
				*/

#define BUFF_ERR     (-75)	/* buffer error */

/* -----------------------------------------------------------------------*/

#define NULL		0

/* Protocol Data Units */

#define LR		1
#define LD		2
#define LT		4
#define LA		5
#define LN		6
#define LNA		7

/* Protocol Data Units Formats (Offsets) */

#define PDU_LI		0		/* header length indicator */
#define PDU_TYPE	1		/* type of pdu */

/***************************************************************************
*									   *
*			Buffer Management Definitions			   *
*									   *
***************************************************************************/


/* each blst (buffer list) structure controls an actual buffer */

struct BLST {
	USIGN_16 mark;			/* 0: free | 1: used */
	USIGN_8 *bptr;			/* pointer to the actual buffer */
	USIGN_16 len;			/* lenght of data in buffer */
	struct BLST *next_b;		/* pointer to next structure */
};

/* each buffer structure controls a linked list of blst structures */

struct BUFFER {
	struct BLST *list;		/* pointer to head node */
	struct BLST *free;		/* pointer to node with free buffer */
	USIGN_16 num;			/* total number of nodes (buffers) */
	USIGN_16 used;			/* number of buffers being used */
	struct BLST *used_lst;		/* pointer to head of list
					  of buffers in use */
	struct BLST *next_lst;
};

#define BIT0_ON 	(0x01)

/***************************************************************************

     				LLVL Definitions

***************************************************************************/

/* Link Disconnect reason code */
#define PROT_ERR		1
#define PROT_LVL_MISMATCH	2
#define BAD_LR_PARMS		3
#define RETRAN_TMR_EXP		4
#define INACT_TMR_EXP		5

struct link_ctl_blk {
	/* variables initialized at one-time initialization */
	USIGN_8 status_1;		/* link status */
	USIGN_8 status_2;		/* link status */
	USIGN_8 status_3;		/* link status */
	USIGN_16 ack_timer;		/* acknowledgement timer value */
	USIGN_16 max_data_sz;	/* negotiated maximum user data size */
	USIGN_16 prot_level; 	/* negotiated protocol level */
	USIGN_16 lt_tmr;		/* lt (data) retransmission timer value */
	USIGN_16 srv_class;		/* local service class */
	USIGN_16 window_sz;		/* negotiated window size */
	USIGN_16 window_tmr; 	/* flow control window timer value */
	USIGN_8 baud_rate;		/* current link baud rate */
	USIGN_8 ln_rsn;		/* last ln (remote) receive sequence number */
	USIGN_8 ln_ssn;		/* last ln (local) send sequence number */

	/* variables initialized/reset at link reset */
	USIGN_8 ltssn_acked;	/* last lt sent seq. # that has been acked */
						/* by remote link */
	USIGN_8 ltrsn_acked;	/* last lt received seq. # that has been */
				/* acked by local link */
	USIGN_8 lt_rsn;		/* last lt (remote) received sequence number */
	USIGN_8 lt_ssn;		/* last lt (local) sent sequence number */
	USIGN_16 ln_ret_cnt; 	/* ln retransmission count */
	USIGN_16 lt_ret_cnt; 	/* lt retransmission count */

	/* variables initialized at start of data phase */
	USIGN_16 ack_threshold;	/* acknowledgement threshold */
	USIGN_16 lcl_credit; 	/* receive (local) credit */
	USIGN_16 rem_credit; 	/* send (remote) credit */

	/* other variables */
	USIGN_16 l_disc_code;	/* link layer disconnect code */
	USIGN_16 ln_rtype;		/* ln lpdu type received */
	USIGN_16 ln_stype;		/* ln lpdu type to be sent */
	USIGN_16 lpdu_type;		/* type of last lpdu received */
	USIGN_16 u_disc_code;	/* user disconnect code */
	USIGN_8 lr_parm;		/* lr parameter flag */
};

/* Link Status Byte 1 bit definition (lcb.status_1)
*/
#define LA_RECEIVED	1	/* on: la lpdu received */
#define LINK_EST	2	/* on: link established */
#define DATA_READY	4	/* on: user data available (lt received) */
#define ACK_TIMER	8	/* on: timer set */
#define RET_TIMER	16	/* on: retran timer set */
#define MODE		32	/* on: acceptor º off: initiator */
#define HDUPLEX 	64	/* on: half duplex º off: full duplex */
#define ACK		128	/* on: ack needed - normal ack sequence */

/* Link Status Byte 2 bit definition (lcb.status_2)
*/
#define FORCE_ACK	1	/* on: force ack needed - lt received bad */
#define FORCE_RET	2	/* on: lt retran needed - la received bad */
#define ZERO_WNDW	4	/* on: zero window in effect */
#define WNDW_TIMER	8	/* on: flow control window timer set */

/* Link Status Byte 3 bit definition (lcb.status_3)
*/
#define LN_RECEIVED	1	/* on: ln lpdu received */
#define LNA_RECEIVED	2	/* on: lna lpdu received */
#define FORCE_LNA	4	/* on: need to send an lna lpdu */
#define LN_TIMER	16	/* on: ln retran timer set */
#define DUP_IGNORED	32	/* on: received an immediate duplicate LT */
				/*     it was ignored */
#define LN_SENT	64	/* on: ln lpdu sent, not acked */

/* timer event flag definition */
#define FRAME_SND	1	/* frame has been sent */
#define FRAME_RCV	2	/* a frame has been received */
#define FRAME_DN	3	/* frame sent, including end flag */

/* offsets of LPDU */
#define LT_SEQ		4	/* sequence number in LT */

/* Stream link buffer definitions */
#define RBUF_LEN	576
#define SBUF_LEN	1024

/***************************************************************************
*									   *
*			MNP Control Block Definitions			   *
*									   *
***************************************************************************/

struct MNP_CB {
	SIGN_16 (*l_connect)();	/* function to establish a link connection */
	SIGN_16 (*l_disconnect)();	/* function to disconnect a link connection */
	SIGN_16 (*l_link)();	/* function to maintain link */
	SIGN_16 (*lt_send)();	/* function to send an lt lpdu */
	SIGN_16 (*lt_receive)();	/* function to receive an lt lpdu */
	SIGN_16 (*l_update)();	/* function to change link variables */
	SIGN_16 (*ret_rcvbuf)();	/* function to return a receive buffer */
	struct BUFFER *rb;	/* addr of struct to control receive buffers */
	struct BUFFER *ftb;	/* addr of struct to control transmit buffers */
	struct BUFFER *rlkb;	/* addr of struct to control lnk rec buffers */
	struct BUFFER *lkb;	/* addr of struct to control lnk xmit buffers */
	SIGN_16 ld_reason;		/* reason code in the received LD */
	USIGN_16 ld_source;		/* disc source: 0=layer, 1=user */
	USIGN_16 parity;		/* stream link parity type */
};
