/*******************************************************************

			LLVL.H Header File for LLVL.C

	10/29/87 - explicit data typing..gp

*******************************************************************/

#define STRM_WNDW_SZ	8	/* window size in stream mode */
#define STRM_DATA_SZ   64	/* max data size in stream mode */
				/* 62 bytes of data */
				/* 2 bytes reserved */
#define STRM_FDAT_SZ   62

#define LCL_SCLASS	2	/* local service class: async, full-duplex, BSC-like framing */

/* all timers have value of 1 greater than the MNP specs */
/* this is to be sure they are never less than what the specs say */

#define LTTMR_24	5	/* lt retran timer for 2400 baud */
#define W_TMR_24	4	/* window timer for 2400 baud */

#define LTTMR_12	7	/* lt retran timer for 1200 baud */
#define W_TMR_12	8	/* window timer for 1200 baud */

#define LTTMR_3 	22	/* lt retran timer for 300 baud */
#define W_TMR_3 	32	/* window timer for 300 baud */

#define LTTMR_110	58	/* lt retran timer for 110 baud */
#define W_TMR_110	87	/* window timer for 300 baud */

#define LR_TMR		10	/* value for LR retran timer */

#define LR_TRAN_CNT	2	/* number of times to transmit an LR */
#define LR_RETRAN_CNT	(LR_TRAN_CNT - 1)	/* number of times to REtransmit an LR */
#define LA_WAIT_CNT	2	/* number of times to wait for an LA */
#define RET_LIMIT	12	/* limit in retransmitting an LT */


/* LR parameters bit definition ((mnpcb->lcb)->lr_parm) */
#define LR_SRV_CLASS	1	/* lr received contains service class */
#define LR_WNDW_SZ	2	/* lr received contains window size */
#define LR_DATA_SZ	4	/* lr received contains max data size */

#define LR_ILEN 	20	/* length of an initiator's LR */
#define LR_ALEN 	10	/* basic length of an acceptor's LR - no parms 2, 3 & 4 yet */
#define LD_LEN		4
#define LA_LEN		7
#define LN_LEN		7
#define LNA_LEN 	4
#define LTH_LEN 	4

/* Link state variables */
#define IDLE			0
#define LR_RESP_WAIT		1
#define PARMS_NEGO		2
#define LR_WAIT 		3
#define CONNECT_REQ_WAIT	4
#define LA_WAIT 		5
#define LNK_CONNECTED		6
#define LNK_ERROR		7

#define PROTOCOL_ERR		1
#define PROT_LVL_MISMATCH	2
#define BAD_LR_PARMS		3
#define RETRAN_TMR_EXP		4
#define INACT_TMR_EXP		5

#define L_ACCEPTOR		mode
#define SETBIT1(bit)		lcb.status_1 |= bit;		/* used to set bit */
#define SETBIT2(bit)		lcb.status_2 |= bit;
#define SETBIT3(bit)		lcb.status_3 |= bit;
#define CLRBIT1(bit)		lcb.status_1 &= ~(bit); 	/* used to clear bit */
#define CLRBIT2(bit)		lcb.status_2 &= ~(bit);
#define CLRBIT3(bit)		lcb.status_3 &= ~(bit);
#define BIT1SET(bit)		(lcb.status_1 & bit)		/* used to test if bit is set */
#define BIT2SET(bit)		(lcb.status_2 & bit)
#define BIT3SET(bit)		(lcb.status_3 & bit)

#define WAIT		1
#define NOWAIT		0
#define B_2400		5		/* code for 2400 baud */
#define B_1200		4		/* code for 1200 baud */
#define B_300		2		/* code for 300 baud */
#define B_110		1		/* code for 110 baud */
#define RF_INIT 	0
#define SF_INIT 	0
#define MAX_LPDU_SZ	40
#define BLK_DATA_SZ 260

/* send framer variables */
extern USIGN_16 frame_snt,
			frame_dne;
extern USIGN_16 sf_busy,			/* set by send_pdu, cleared by send framer */
	     	sf_lt,
			sf_len, 		/* cleared by send_pdu, if sending LD */
			sf_state;		/* initialized by link_reset */
extern USIGN_16 modem_out_busy;
extern void trigger_sf();
extern USIGN_8 *sf_ptr;

/* receive framer variable */
extern USIGN_16 frame_rcvd;
extern USIGN_16 rf_busy,			/* set and cleared by rcv_framer */
			rf_bupdate,
	   		rf_state, 
			rdle_flg;
extern struct BUFFER *p_rb, 
				*p_ftb,
				*p_rlkb;

extern USIGN_16 ack_tm,
			fcw_tm,
			inact_tm,
			lr_tm,
			lt_tm, 
			ln_tm;
extern USIGN_16 baudrate;
extern struct MNP_CB *p_mnpcb;
