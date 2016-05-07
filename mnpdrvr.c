/*=====================================================================

                      The Microcom MNP Library
        				(Microsoft C Version)


	11/4/87 - explicit data typing...gp
	8/27/87 - rcv_lt() modified to use lcb.lcl_credit..gp

-----------------------------------------------------------------------

        LDRIVER - MNP Link RS-232 Interrupt Service Routines

     This module implements asynchronous I/O for IBM PC-compatible
     machines and thus much of this code may not be portable to 
     other dissimilar environments.

     This code implements Class 2 of the MNP link protocol,
     i.e. it uses the stop/start byte-oriented framing technique.
     A Class 3 implementation is not possible in the IBM PC 
     environment since the communications hardware is not capable
     of switching to synchronous (bit-oriented) operation.
     Protocol messages and procedures, however, are independent
     of the framining technique.		

     Global routines:

	- ascode: service I/O interrupt 
	- lne_stat: check carrier status 
	- trigger_sf: initiate transmit interrupts

=====================================================================*/

/* Header files
*/
#include <dos.h>
#include <mnpdat.h>

/* Interrupt-related definitions
*/
#define INT_8259A_PORT	0x20
#define EOI		0x20
#define RCV_DATA	4
#define THR_EMPTY	2
#define MODSTAT_CHNG	0

/* Send framer state names
*/
#define SF_INIT 	0
#define SF_DLE		1
#define SF_STX		2
#define SF_INFO 	3
#define SF_DLEINFO	4
#define SF_ETX		5
#define SF_FCS1 	6
#define SF_FCS2 	7

/* Receive framer state names
*/
#define RF_INIT 	0
#define RF_DLE		1
#define RF_STX		2
#define RF_HDLEN	3
#define RF_HEADER	4
#define RF_INFOL	5
#define RF_INFO 	6
#define RF_FCS1 	7
#define RF_FCS2 	8

/* Frame octet definitions
*/
#define SYN		0x16
#define DLE		0x10
#define ETX		3
#define STX		2

/* Length of header buffer (maximum acceptable LPDU header)
*/
#define MAX_HDRLEN	100

/* Bit set, clear and test definitions
*/
#define SETBIT1(bit)	lcb.status_1 |= bit;	   
#define SETBIT2(bit)	lcb.status_2 |= bit;
#define SETBIT3(bit)	lcb.status_3 |= bit;
#define CLRBIT1(bit)	lcb.status_1 &= ~bit;	     
#define CLRBIT2(bit)	lcb.status_2 &= ~bit;
#define CLRBIT3(bit)	lcb.status_3 &= ~bit;
#define BIT1SET(bit)	(lcb.status_1 & bit)	     
#define BIT2SET(bit)	(lcb.status_2 & bit)
#define BIT3SET(bit)	(lcb.status_3 & bit)

/* External references
*/
extern USIGN_16 iir_add;			/* int id reg address */
extern USIGN_8 linestat;		/* line status var */
extern USIGN_16 fcw_tm;			/* window timer */
extern struct link_ctl_blk lcb;  /* link control block */
extern struct BUFFER rb, ftb, rlkb;  /* buffers */
extern USIGN_8 *rb_iptr;		/* rcv buf insert pointer */
extern USIGN_16 rb_cnt,tbcnt;		/* buffer counts */
extern USIGN_8 rbuf[RBUF_LEN];	/* receive buffer */
extern USIGN_16 ln_tm;

/* Function definitions
*/
void snd_framer();
void rcv_framer();
void mod_stat();
void rcv_lt();
void rcv_la();
void rcv_lna();
void rcv_ln();

/* Local data
*/
USIGN_8 *sf_ptr,
     *rf_hptr,
     *rf_dptr;
USIGN_8 snd_char,
     rcv_char;
struct 
	{
	USIGN_8 low;
	USIGN_8 hi;
	}
		snd_fcs,			/* send fcs */
  		rcv_fcs,			/* calculated receive fcs */
  		rfcs;			/* received fcs */
USIGN_16 rdle_flg;		/* 'previous char DLE' flag */
USIGN_16 rf_hdrlen,				/* receive header length  */
    hdrcnt,			
    rf_datlen,
    sf_state,				/* send framer state var */
    rf_state,				/* receive framer state var */
    sf_len,
    sf_busy,				/* 'send frame' flag */
    sf_lt,				/* 'sending LT' flag */
    frame_snt,				/* 'info field sent' flag */ 
    frame_rcvd,				/* 'frame received' flag */
    frame_dne;				/* 'frame sent' flag */
USIGN_8 *rf_tiptr;

struct MNP_CB *p_mnpcb;
struct BLST *dat_struct, *hdr_struct;

USIGN_16 modem_out_busy;	/* RS-232 hardware status flag
					          0=transmitter not active
					          1=transmitter active */

/* Function declarations
*/
USIGN_8 get_char();

/*GLOBAL***************************************************************

	ascode - MNP RS-232 interrupt service routine

     This routine is called by 'async' (in module 'async.asm')
     to service an async I/O interrupt.

**********************************************************************/

ascode()
{

USIGN_8 int_id_reg;			/* value in int id reg */

/* Handle interrupts for as long as there are any. 
*/
for (;;)
	{

/* Read the Interrupt Identification Register.  If bit 0 is on, 
** there is no interrupt.  In this case cancel the interrupt by writing ** the "end-of-interrupt" byte to the Oper Ctl Word 2 and return. 
*/
	if ((int_id_reg = inp(iir_add)) & BIT0_ON)
		{
		outp(INT_8259A_PORT, EOI);
		return;
		}

/* Since bit 0 is off, there is an interrupt.  Take action based
** on the type of interrupt. 
*/
	switch (int_id_reg)
		{

/* Receive buffer full interrupt
*/
		case RCV_DATA:				
			rcv_framer();
			break;

/* Transmit buffer empty interrupt
*/
		case THR_EMPTY:
			snd_framer();
			break;

/* Modem status change interrupt
*/
		case MODSTAT_CHNG:			
			mod_stat();
			break;
		}
	}
}

/*GLOBAL***************************************************************

	lne_stat - physical-connection status routine

**********************************************************************/

SIGN_16 lne_stat()
{

/* If the carrier detect bit is 1, return success (physical-connection
** still active).  Otherwise, return with a failure indication. 
*/
if (linestat & 0x80)
    return (SUCCESS);
else
    return (1);
}

/*GLOBAL***************************************************************

	trigger_sf - initiate send framer

**********************************************************************/

void trigger_sf()
{

extern USIGN_16 port_add;

/* If the RS-232 transmitter is not already busy, then a byte must 
** be placed in the transmit buffer to begin interrupt-driven sending.
** This is the start of a frame and thus the first byte to be sent is
** the SYN which begins the frame start flag.  Also the send framer's
** state variable is initialed here.
*/ 
if (!modem_out_busy)
	{
	modem_out_busy = 1;
	sf_state = SF_DLE;				
	outp(port_add, SYN);
	}
}

/*LOCAL----------------------------------------------------------------

	get_char - get byte from send buffer

---------------------------------------------------------------------*/

USIGN_8 get_char()
{

/* Return the byte at the current send framer pointer position.
** In the process, advance the point for next time and reduce the
** number of bytes remaining by one.
*/
sf_len--;					
return (*sf_ptr++);
}

/*LOCAL----------------------------------------------------------------

	mod_stat - modem status change interrupt service routine

---------------------------------------------------------------------*/

void mod_stat()
{

/* Read modem status register and save the value.
*/
linestat = inp(iir_add + 4);

/* If carrier has been lost, signal the mainline that the link
** also has been lost by lowering the 'link established' flag. 
*/
if (!(linestat & 0x80))
    CLRBIT1(LINK_EST)	

}

/*LOCAL----------------------------------------------------------------

	put_char - store an LT data byte

---------------------------------------------------------------------*/

void put_char()
{

/* Ignore this byte if the amount of received data exceeds the
** negotiated maximum user data size for the LT LPDU.  This could occur
** if the frame is broken or if the correspondent is misbehaving. 
*/
if (rf_datlen >= lcb.max_data_sz)
	return;

/* Otherwise, accept this byte. 
*/
rf_datlen++;				
fcscalc (&rcv_fcs, rcv_char);

/* Now store the data byte in the receive buffer using the temporary
** insert pointer.  Advance the pointer, checking for wrap.
*/
*rf_tiptr++ = rcv_char;			
if (rf_tiptr >= rbuf + RBUF_LEN)
	rf_tiptr = rbuf;

}

/*LOCAL----------------------------------------------------------------

	put_hdr - store an LPDU header byte

---------------------------------------------------------------------*/

void put_hdr()
{

/* Store header byte in header buffer.  Increment count of bytes in
** the buffer and include the byte in the receive fcs.
*/
rf_hdrlen++;
*rf_hptr++ = rcv_char;			      
fcscalc (&rcv_fcs, rcv_char);

}

/*LOCAL----------------------------------------------------------------

	RCV_FRAMER - Receive Framer

---------------------------------------------------------------------*/

void rcv_framer()
{

extern USIGN_16 port_add;

/* Get received data byte and save it for later
*/
rcv_char = inp(port_add);

/* Take action based on current state of receive finite state machine.
** 'rf_state' is the state variable.
*/
switch (rf_state)
	{

/* State RF_INIT - This is the initial state.  Search the received
** data stream for a SYN character which may be the beginning
** of the start flag sequence of a byte mode frame. 
*/
	case RF_INIT:
		if (rcv_char == SYN)
			rf_state++;	
		break;

/* State RF_DLE - Search for the second byte in the start flag, a DLE
** byte.  Go back to the initial state (SYN search) if this byte is
** something other than a DLE.
*/
	case RF_DLE:
		if (rcv_char == DLE)
			 rf_state++; 
		else 
			rf_state--;		
		break;

/* State RF_STX - If the received byte is an STX then a frame start flag
** has been found.	In this case, initialize to receive an LPDU.
** Otherwise, go back to looking for a SYN. 
*/
	case RF_STX:
		if (rcv_char == STX)
			{
			get_b(&rlkb,&hdr_struct);
			rf_hptr = hdr_struct->bptr;
			hdrcnt=rf_hdrlen=rf_datlen=rdle_flg=0;
			rcv_fcs.hi=rcv_fcs.low=NULL;
	    		rf_state++; 		
	    		}
		else				
	    		rf_state = RF_INIT; 	    
		break;

/* State RF_HDLEN - Get the first byte of the LPDU, the length
** indicator.  Wait for the next byte if this happens to be a DLE (in a
** good frame the DLE will be doubled).  If the value  received is
** greater than this implementation's maximum header length, it must be
** a protocol error or the value is broken.  In either case, ignore this
** LPDU. This is like a broken frame, so force the sender to ack. 
*/
	case RF_HDLEN:
		if ((rcv_char != DLE) || ((rcv_char == DLE) && rdle_flg))
			{
			rdle_flg = 0;		
		
			if (rcv_char > MAX_HDRLEN)
				{
				ret_b(&rlkb, hdr_struct);	
				SETBIT2(FORCE_ACK)
				rf_state = RF_INIT;		
				}
	    		else
				{
				hdrcnt = rcv_char;
				put_hdr();
				rf_state++;
				}
	    		}
		else
	    		rdle_flg = 1;
		break;

/* State RF_HEADER - Receive header bytes, assuming the length indicator
** was correct.  Any bytes following the header will temporarily be
** assumed to be data.
*/
	case RF_HEADER:
		if ((rcv_char != DLE) || ((rcv_char == DLE) && rdle_flg)) 
			{
	    		rdle_flg = 0;
	    		put_hdr();
	    		if (!(--hdrcnt))
				rf_state++;
	    		}
		else
	    		rdle_flg = 1;
		break;

/* State RF_INFOL -  Receive first data byte.  Note that we must check
** for the stop flag just in case the length indicator byte was broken.  ** When the first data byte is received, the temporary insert pointer
** into the data receive buffer is initialized to the current location
** of the real pointer.
*/
	case RF_INFOL:
		if ((rcv_char != DLE) || ((rcv_char == DLE) && rdle_flg))
			{
	    		if ((rcv_char == ETX) && rdle_flg) 
				{
				fcscalc(&rcv_fcs, rcv_char);
				rf_state = RF_FCS1;
				}
	    		else
				{
				rdle_flg = 0;
                	rf_tiptr = rb_iptr; 		
	        		put_char();
				rf_state++; 		   
				}
			}
		else
	    		rdle_flg = 1;
		break;

/* State RF_INFO - Receive data bytes until the stop flag is found.
*/
	case RF_INFO:
		if ((rcv_char != DLE) || ((rcv_char == DLE) && rdle_flg))
			{
	    		if ((rcv_char == ETX) && rdle_flg)
				{
				fcscalc(&rcv_fcs, rcv_char);
				rf_state++;		
				}
	    		else
				put_char();
	    		rdle_flg = 0;
	    		}
		else
	    		rdle_flg = 1;
		break;

/* State RF_FCS1 - Receive first byte of fcs.
*/
	case RF_FCS1:
		rfcs.hi = rcv_char;
		rf_state++;
		break;

/* State RF_FCS2 - Receive second (low) byte of fcs and take end-of-
** frame action.  If the calculated fcs is not identical to that
** received, the received frame is "broken".  Signal this to the sender
** by setting the LPDU type to 0 and setting the FORCE_ACK flag.  If the
** FCS's match, take action based on the type of LPDU received. 
*/
	case RF_FCS2:
		if ((rcv_fcs.hi != rfcs.hi) || (rcv_fcs.low != rcv_char))
			{
	    		lcb.lpdu_type = 0;
	    		SETBIT2(FORCE_ACK);
	    		ret_b(&rlkb, hdr_struct);
	    		}
		else
			{
	    		switch (lcb.lpdu_type = *(hdr_struct->bptr + PDU_TYPE))
				{
				case LT:
		    			rcv_lt();
		    			break;

				case LA:
		    			rcv_la();
		    			break;

/* Link Request - If an LR arrives during what we think is the data
** phase, the other side may be retransmitting. In this case, just
** discard the LR.  Otherwise, save the LR for the mainline to process.
** In either case, signal the mainline to ack. 
*/
				case LR:
		    			if (BIT1SET(LINK_EST))
						ret_b(&rlkb, hdr_struct);
		    			else
						rlkb.used_lst = hdr_struct;  
		    			SETBIT2(FORCE_ACK)
		    			break;

				case LN:
		    			rcv_ln();
		    			break;

				case LNA:
		    			rcv_lna();
		    			break;

/* Link Disconnect - Signal link termination to the mainline.  If the LD
** contains a user reason byte (signalled by parm 1 having a value of
** 255), save it in the LCB. 
*/
				case LD:
		    			CLRBIT1(LINK_EST)
		    			if (*(hdr_struct->bptr + 4) == 255)
						{
						p_mnpcb->ld_source = 1;
						p_mnpcb->ld_reason = 
							*(hdr_struct->bptr+7);
						}
		    			else
						p_mnpcb->ld_source = 0;
		    			break;
				}
	    		}

/* Signal the mainline that a frame has been received and reset the
** receive frame state machine for the next frame. 
*/
	    		frame_rcvd = 1;			
	    		rf_state = RF_INIT; 	
	    		break;
		}

}

/*LOCAL----------------------------------------------------------------

	rcv_la - process a received LA LPDU

---------------------------------------------------------------------*/

void rcv_la()
{

register USIGN_8 *p;
USIGN_16 i;
USIGN_8 lt_ret_seq;

/* Ignore this LA LPDU if a destructive attention is in progress. 
*/
if (BIT3SET(LN_SENT))
	{
	ret_b(&rlkb,hdr_struct);
    	return;
    	}

/* For reasons of robustness, check the validity of the LA LPDU 
*/
p = hdr_struct->bptr + 2;			/* point to parm 1 */
if ((*p++ != 1) || (*p != 1))
	{
	ret_b(&rlkb,hdr_struct);
	return;
	}
p += 2;
if ((*p++ != 2) || (*p != 1))
	{
	ret_b(&rlkb,hdr_struct);
	return;
	}

/* If the receive sequence number of this LA is the same as the last
** one received, it may be an implied negative acknowledgment.  Force
** the mainline to consider retransmission by setting the 'force
** retransmission' flag.  Otherwise, save the new number as the last
** acked and set the 'good ack received' flag. 
*/
p -= 2;
if (*p == lcb.ltssn_acked)
	SETBIT2(FORCE_RET)
else
	{
	lcb.ltssn_acked = *p;
    	SETBIT1(LA_RECEIVED)
    	}

/* Process the new credit information received. If it is zero, return.
*/
p += 3;
if ((lcb.rem_credit = *p) == 0)
	{
	ret_b(&rlkb,hdr_struct);
    	return;
    	}

/* If credit is not zero and this ack was a good one, take into account
** the LT LPDUs that may have been sent since the correspondent sent
** this LA.  That is, subtract from the credit the number of these not
** yet acked LTs. 
*/
if (BIT1SET(LA_RECEIVED) && tbcnt)
	{
	lt_ret_seq = *(ftb.used_lst->bptr + LT_SEQ);

	if (lt_ret_seq <= lcb.ltssn_acked)
		i = (lcb.ltssn_acked - lt_ret_seq) + 1;
	else
		i = (256 - lt_ret_seq) + lcb.ltssn_acked + 1;

    	i = tbcnt - i;
    	if (lcb.rem_credit >= i)
		{
		lcb.rem_credit -= i;
        	if (lcb.rem_credit < 0)
			lcb.rem_credit = 0;
        	}
    	else
		lcb.rem_credit = 0;
    	}

/* Clean up 
*/
ret_b(&rlkb, hdr_struct);

}

/*LOCAL----------------------------------------------------------------

	rcv_ln - process a received LN LPDU

---------------------------------------------------------------------*/

void rcv_ln()
{

register USIGN_8 *p;

/* Check the sequence number of this LN LPDU.  If it is the next one
** in the sequence space, signal its arrival to the sender by
** setting the LN_RECEIVED flag.  Also remember the type of the LN LPDU
** by saving the type parameter value in the LCB. 
*/
p = hdr_struct->bptr + 4;
if (*p == (USIGN_8) ++lcb.ln_rsn)
	{
	lcb.ln_rtype = *(p + 3);
    	SETBIT3(LN_RECEIVED)
    	}

/* If this is not the next one, then decrement the receive sequence
** number which was incremented above and signal the need to send an
** LNA LPDU.
*/
else
	{
	lcb.ln_rsn--;
	SETBIT3(FORCE_LNA)
	}

ret_b(&rlkb, hdr_struct);
}

/*LOCAL----------------------------------------------------------------

	rcv_lna -  process a received LNA LPDU

---------------------------------------------------------------------*/

void rcv_lna()
{

register USIGN_8 *p;

/* If the LNA LPDU is an acknowledgment of an outstanding LN, its
** sequence number will match that of the last LN sent.	In this
** case, signal the sender that the LN has been acknowledged.  
** Otherwise, just ignore this LPDU. 
*/
if (*(hdr_struct->bptr + 4) == lcb.ln_ssn)
	{
	lcb.ln_ret_cnt = NULL;
	CLRBIT3(LN_SENT)
	CLRBIT3(LN_TIMER)
	ln_tm=NULL;	
	}
ret_b(&rlkb, hdr_struct);
}

/*LOCAL----------------------------------------------------------------

	rcv_lt - process a received LT LPDU

---------------------------------------------------------------------*/

void rcv_lt()
{

/* Ignore this LT LPDU if a destructive attention is in progress. 
*/
if (BIT3SET(LN_SENT))
	{
	ret_b(&rlkb, hdr_struct);
    	return;
    	}

/* Reset window timer, if window timer is enabled. 
*/
if (BIT2SET(WNDW_TIMER))
	fcw_tm = lcb.window_tmr;

/* Accept this LT LPDU if it is the next one in the sequence space
** and there is room for it (i.e. there is a receive credit).
*/
if ((lcb.lcl_credit > 0)
	 && (*(hdr_struct->bptr + LT_SEQ) == (USIGN_8) lcb.lt_rsn + 1))
	{ 
	++lcb.lt_rsn;
	--lcb.lcl_credit;
	CLRBIT3(DUP_IGNORED)
    	rb_iptr = rf_tiptr;	
    	rb_cnt = rb_cnt + rf_datlen;
    	}

/* Otherwise, ignore the LT LPDU. In this case, set the FORCE_ACK flag
** to get the sender to ack as rapidly as possible.
*/
else
	{
    	SETBIT2(FORCE_ACK)

/* If the LT LPDU is the same as the last one which arrived, i.e. it is
** an immediate duplicate, and this is the first occurrence of the
** immediate duplicate, then don't force the sender to ack this time. 
*/
	if ((*(hdr_struct->bptr + LT_SEQ) == lcb.lt_rsn)
		&& !BIT3SET(DUP_IGNORED))
		{
		CLRBIT2(FORCE_ACK)
		SETBIT3(DUP_IGNORED)		
		}
    	}

/* Clean up 
*/
ret_b(&rlkb, hdr_struct);
}

/*LOCAL----------------------------------------------------------------

	snd_framer - handle a transmit holding register empty
		        interrupt.  This subroutine sends a byte-mode frame.

---------------------------------------------------------------------*/

void snd_framer()
{

extern USIGN_16 port_add;

/* Take action based on the current state of the framer state machine. 
*/
switch (sf_state)
	{

/* State SF_INIT - initial state.  If the mainline has designated an
** LPDU to frame, the sf_busy flag will be true.  If this is the case,
** begin the start flag of the frame. 
*/
    case SF_INIT:
	if (sf_busy)
		{
		snd_char = SYN;
		sf_state++;
		break;
		}
	else
		{
		frame_dne = 1;
		modem_out_busy = 0;
		return;
		}

/* State SF_DLE - Send DLE of start flag. 
*/
	case SF_DLE:
		snd_fcs.hi = snd_fcs.low = 0;
		snd_char = DLE;
		sf_state++;				
		break;

/* State SF_STX - Send STX of start flag. 
*/
	case SF_STX:
		snd_char = STX;
		sf_state++;
		break;

/* State SF_INFO -  As long as there are info field bytes, send a 
** byte and double DLE's as necessary.  At the end of the information
** field of the frame, start the end flag.   Signal the mainline code
** that it can enqueue another frame if it is already waiting on
** 'frame_snt'.  Clearing 'sf_busy' will cause the framer to stop after
** the end flag unless another frame has been enqueued. 
*/
	case SF_INFO:
		if (sf_len > 0)
			{
			if ((snd_char = get_char()) == DLE)
			sf_state++;
			fcscalc(&snd_fcs, snd_char);
			}
		else
			{
			sf_busy = sf_lt = 0;
			frame_snt = 1;
			snd_char = DLE;
			sf_state = SF_ETX;
			}
		break;

/* State SF_DLEINFO - Double an info field DLE.
*/
	case SF_DLEINFO:
		snd_char = DLE;
		sf_state--;
		break;

/* State SF_ETX - Send ETX of end flag.
*/
	case SF_ETX:
		snd_char = ETX;
		fcscalc(&snd_fcs, snd_char);
		sf_state++;
		break;

/* State SF_FCS1 - Send first byte of frame check sequence.
*/
	case SF_FCS1:
		snd_char = snd_fcs.hi;
		sf_state++;
		break;

/* State SF_FCS2 - Send second byte of frame check sequence.
** After this byte is sent, the send framer goes back to its
** initial state.
*/
	case SF_FCS2:
		snd_char = snd_fcs.low;
		sf_state = SF_INIT;
		break;
	}

/* Send byte out RS-232 port 
*/
outp(port_add, snd_char);

}
