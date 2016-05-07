/*=============================================================================

			         The Microcom MNP Library
					(Microsoft C Version)

-------------------------------------------------------------------------------

		         MNPLLVL - MNP Link Level Routines

-------------------------------------------------------------------------------

     Modification History

	3/25/87 - Compuserve V1.0

=============================================================================*/

/* Header files 
*/
#include <dos.h>
#include <mnpdat.h>
#include <llvl.h>

/* External references
*/
extern USIGN_16 sb_cnt;

/* Global buffers 
*/
struct link_ctl_blk lcb;

struct BUFFER rb, ftb, lkb, rlkb;

struct BLST lkblst[2],
	    rlkblst[2],
	    iatblst[8];

USIGN_8 lkbuff[MAX_LPDU_SZ * 2];
USIGN_8 rlkbuff[MAX_LPDU_SZ * 2];
USIGN_8 iatbuff[(64+5)*8];

USIGN_16 tbcnt;

USIGN_16 g_tm,
	ack_tm,
	fcw_tm,
	lr_tm,
	lt_tm,
	ln_tm;

USIGN_16 port_add,
	chan_no,
	iir_add;

USIGN_8 linestat;

/* Function declarations
*/
void as_disconnect();
SIGN_16 acking();
SIGN_16 credit_chk();
SIGN_16 lpdu_send();
SIGN_16 parse_lr();
SIGN_16 receive_wait();
SIGN_16 retran_lt();
SIGN_16 send_la();
SIGN_16 send_pdu();
SIGN_16 send_wait();

/*GLOBAL********************************************************************

	as_connect - establish an MNP link-connection

***************************************************************************/

SIGN_16 as_connect (mnpcb,mode)

struct MNP_CB *mnpcb;
USIGN_16 mode;
{

SIGN_16 lr_cnt,
    la_cnt,
    retcode,
    link_state;

/* Initialize
*/
lcb.lpdu_type = 0;
link_state = IDLE;

for (;;)
	{

	switch (link_state)
		{

		case IDLE:
			link_init(mnpcb);
			link_reset();
			if (L_ACCEPTOR)
				{
				SETBIT1(MODE)
				link_state = LR_WAIT;
				}
			else
				{
				suspend(5);
				if (retcode = lpdu_send(LR,WAIT))
					link_state = LNK_ERROR;
				else
					link_state = LR_RESP_WAIT;
				}
			break;

		case LR_RESP_WAIT:
			lr_cnt = LR_RETRAN_CNT;
			retcode = SUCCESS;
			while ((retcode = receive_wait()) || !lcb.lpdu_type)
				{
				if (retcode == NO_PHYSICAL)
					{
					link_state = LNK_ERROR;
					break;
					}
				if (lr_cnt--)
					{
					if (retcode = lpdu_send(LR,WAIT))
						{
						link_state = LNK_ERROR;
						break;
						}
					}
				else
					{
					retcode = TIME_OUT;
					link_state = LNK_ERROR;
					break;
					}
				}

			if (retcode == SUCCESS) 
				link_state = PARMS_NEGO;
			break;

		case PARMS_NEGO:
			if (retcode = parse_lr())
				{
				as_disconnect(retcode,NULL);
				return(retcode + LR_CODE);
				}

			if (retcode = lpdu_send(LA,WAIT))
				link_state = LNK_ERROR;
			else
				link_state = LNK_CONNECTED;
			break;

		case LR_WAIT:
			lr_cnt = 2;
			while ((retcode = receive_wait()) || !lcb.lpdu_type)
				{
				if (retcode == NO_PHYSICAL)
					{
					link_state = LNK_ERROR;
					break;
					}
				if (!(--lr_cnt))
					{
					retcode = TIME_OUT;
					link_state = LNK_ERROR;
					break;
					}
				}

			if (link_state != LR_WAIT)
				break;

			if (retcode = parse_lr())
				{
				as_disconnect(retcode,NULL);
				return(retcode + LR_CODE);
				}

			lr_cnt = LR_TRAN_CNT;
			link_state = CONNECT_REQ_WAIT;
			break;

		case CONNECT_REQ_WAIT:
			if (lr_cnt == NULL)
				{
				retcode = FAILURE;
				link_state = LNK_ERROR;
				break;
				}

			if (retcode = lpdu_send(LR,WAIT))
				link_state = LNK_ERROR;
			else
				{
				lr_cnt--;
				la_cnt = LA_WAIT_CNT;
				link_state = LA_WAIT;
				}
			break;

		case LA_WAIT:
			while ((retcode = receive_wait()) || !lcb.lpdu_type)
				{
				if (retcode == NO_PHYSICAL)
					{
					link_state = LNK_ERROR;
					break;
					}
		    		if (--la_cnt)
					{
					if (retcode = lpdu_send(LR,WAIT))
						{
						link_state = LNK_ERROR;
						break;
						}
					}
				else
					{
					retcode = TIME_OUT;
					link_state = LNK_ERROR;
					break;
					}
				}

			if (link_state != LA_WAIT)
				break;

			switch (lcb.lpdu_type)
				{
				case LA:
					lcb.lt_rsn = NULL;

				case LT:
					link_state = LNK_CONNECTED;
					break;

				case LR:
					ret_b (&rlkb,rlkb.used_lst);
					link_state = CONNECT_REQ_WAIT;
					break;
	
				default:
					retcode = FAILURE;
					link_state = LNK_ERROR;
				}

			break;

		case LNK_ERROR:
			as_disconnect(NULL,NULL);
			return(retcode);

		case LNK_CONNECTED:
			dphase_init();
			SETBIT1(LINK_EST)
			return(SUCCESS);
		}
	}
}

/*GLOBAL***********************************************************************

	as_disconnect - terminate a link-connection

******************************************************************************/

void as_disconnect(lreason,ureason)

SIGN_16 lreason,
	ureason;
{

/* If the link is still up, send an LD LPDU to the other side.  If the
** caller has supplied a reason code (in ureason) then save this code
** in the lcb so that it will be sent in the LD.  Wait for the LD to 
** actually be sent (to the very last byte...). 
*/
if (BIT1SET(LINK_EST))
	{
	if ((lcb.l_disc_code = lreason) == 255)
		lcb.u_disc_code = ureason;
	lpdu_send(LD,NOWAIT);
	while (!lne_stat() && modem_out_busy)
		;
	CLRBIT1(LINK_EST);
	}

/* In any case, wait a bit.
*/
suspend(10);

/* Now remove interrupt handlers - link driver and timers. 
*/
drvr_rem();
timerrem();

}

/*GLOBAL********************************************************************

	as_link - maintain a link-connection

***************************************************************************/

SIGN_16 as_link()
{

SIGN_16 retcode;

/* If the link-connection has failed, return reporting link down. 
*/
if (!BIT1SET(LINK_EST))
	return (LNK_DOWN);

/* If the physical connection has been lost, return reporting p-conn down. 
*/
if (lne_stat())
	return (NO_PHYSICAL);

/* If we support attention service, go see if we have any to process. 
*/
if (lcb.prot_level == 2)
	{
	if (retcode = attn_process())
		return(retcode);
	}

/* Go see if it's time to send an ack. 
*/
if (retcode = acking())
	return(retcode);

/* Try to free up any transmit buffers which have been acked. 
*/
tb_free();

/* Go see if we have to retransmit. 
*/
if (retcode = retran_lt())
	return(retcode);

/* Make sure link is still up one more time 
*/
if (!BIT1SET(LINK_EST))
	return(LNK_DOWN);

/* Exit 
*/
return(SUCCESS);
}

/*LOCAL------------------------------------------------------------------------

	acking - send an LA LPDU if an ack condition is present

-----------------------------------------------------------------------------*/

SIGN_16 acking()
{

SIGN_16 retcode,				/* return code */
    lt_unacked; 				/* number of unacked LTs */

/* If the force ack flag is set, send an LA right away.
*/
clr_int();					/* protect flag access */
if (BIT2SET(FORCE_ACK))			/* reset flag if set */
	{
	CLRBIT2(FORCE_ACK)
	set_int();
	return(send_la());			/* send LA and return */
	}
set_int();

/* If the window timer is enabled and has expired, send an LA. 
*/
if (BIT2SET(WNDW_TIMER) && (fcw_tm == 0))
	return(send_la());

/* If there are unacknowledged LT's, it may be time to send an LA. 
*/
if (lt_unacked = lcb.lt_rsn - lcb.ltrsn_acked)
	{

/* If there is no user data to send (i.e. no reason to put it off)
** send an LA.
*/
	if (sb_cnt == 0)
		return(send_la());

/* If the acknowledgment threshold has been reached, send an LA.
*/
	if (lt_unacked >= lcb.ack_threshold)
		return(send_la());

/* If the ack timer has elapsed, send an LA.  Otherwise, set the timer.
*/
	if (BIT1SET(ACK_TIMER))
		{
		if (ack_tm)
			return(SUCCESS);
		else
			return(send_la());
		}
	else
		{
		SETBIT1(ACK_TIMER)
		ack_tm = lcb.ack_timer;
		return(SUCCESS);
		}
	}

/* If there are no unacknowledged LT's, handle the 'zero window
** opening' case.
*/
else
	{
	if (BIT2SET(ZERO_WNDW) && credit_chk())
		{
		CLRBIT2(ZERO_WNDW)
		return(send_la());
		}
	}

/* Exit 
*/
return(SUCCESS);
}

/*LOCAL------------------------------------------------------------------------

	attn_process - handle break signalling

-----------------------------------------------------------------------------*/

attn_process()
{

SIGN_16 retcode;

/* If the LN timer is set and has expired, it is time to retransmit the
** last LN LPDU sent.  However, if the retransmission limit has been
** reached, then the link is unusable.  Terminate the link and return
** with a function value=retran limit reached.
*/
if (BIT3SET(LN_TIMER) && (ln_tm == 0))
	{
	if (lcb.ln_ret_cnt == RET_LIMIT)
		{
		as_disconnect(RETRAN_TMR_EXP,NULL);
		return(-67);
		}
	if (retcode = lpdu_send(LN,NOWAIT))
		return(retcode);
	++lcb.ln_ret_cnt;
	SETBIT3(LN_SENT)

	clr_int();
	SETBIT3(LN_TIMER)
	ln_tm=lcb.lt_tmr;
	set_int();
	}

/* If an LN has been received, check its type for a destructive break
** signal.  Reset the link on receipt of a destructive break.
*/
clr_int();
if (BIT3SET(LN_RECEIVED))
	{
	CLRBIT3(LN_RECEIVED)			
	if (lcb.ln_rtype == 1)
		{
		link_reset();
		dphase_init();
		}
	set_int();
	if (retcode = lpdu_send(LNA,NOWAIT))
		return(retcode);
	}
set_int();

/* Check for need to send an LNA
*/
if (BIT3SET(FORCE_LNA))
	{
	CLRBIT3(FORCE_LNA)
	if (retcode = lpdu_send(LNA,NOWAIT))
		return(retcode);
	}

/* Exit 
*/
return(SUCCESS);
}

/*LOCAL------------------------------------------------------------------------

	credit_chk - determine local ability to receive

-----------------------------------------------------------------------------*/

SIGN_16 credit_chk()
{

extern USIGN_16 rb_cnt;

/* Compute the number of LT LPDUs which can be received.  This is
** the number of max sized LT's which will fit into the receive
** ring buffer.  This number can not be larger than maximum window
** (8 for this implementation).
*/
lcb.lcl_credit = (RBUF_LEN - rb_cnt) / lcb.max_data_sz;
if (lcb.lcl_credit > lcb.window_sz)
	lcb.lcl_credit = lcb.window_sz;

/* Exit.  Function value is receive credit. 
*/
return(lcb.lcl_credit);
}

/*LOCAL------------------------------------------------------------------------

	dphase_init - perform data phase initialization

-----------------------------------------------------------------------------*/

dphase_init()
{

SIGN_16 i;

/* Set initial credits equal to negotiated maximum credit. 
*/
lcb.lcl_credit = lcb.rem_credit = lcb.window_sz;

/* Compute ack threshold 
*/
lcb.ack_threshold = (i = lcb.window_sz / 2) ? i : 1;

/* Be sure that ack conditions are reset. 
*/
CLRBIT1(LA_RECEIVED)
CLRBIT2(FORCE_ACK)

/* If window == 1, lt retran timer is specified to be 8 seconds
*/
if (lcb.window_sz == 1)
	lcb.lt_tmr = 8;
else
	{
	SETBIT2(WNDW_TIMER)
	fcw_tm = lcb.window_tmr;
	}
}

/*LOCAL------------------------------------------------------------------------

	link_init - perform one-time link initialization

-----------------------------------------------------------------------------*/

link_init(mnpcb)

register struct MNP_CB *mnpcb;
{

/* Initialize link control block values 
*/
lcb.status_1 = lcb.status_2 = lcb.status_3 = NULL;

ftb.num = lcb.window_sz = STRM_WNDW_SZ;
lcb.max_data_sz = STRM_DATA_SZ;

lcb.prot_level = 2;
lcb.srv_class = LCL_SCLASS;

lcb.ln_rsn = lcb.ln_ssn = lcb.ln_ret_cnt = ln_tm = NULL;

switch (lcb.baud_rate = baudrate)
	{
	case B_1200:
		lcb.lt_tmr = LTTMR_12;
		lcb.window_tmr = W_TMR_12;
		break;

	case B_300:
		lcb.lt_tmr = LTTMR_3;
		lcb.window_tmr = W_TMR_3;
		break;

	case B_110:
		lcb.lt_tmr = LTTMR_110;
		lcb.window_tmr = W_TMR_110;
		break;

	case B_2400:
	default:
		lcb.lt_tmr = LTTMR_24;
		lcb.window_tmr = W_TMR_24;
		break;
	}

lcb.ack_timer = (lcb.lt_tmr/2)+1;

/* Initialize receive and transmit buffers 
*/
rlkb.list = rlkblst;
rlkb.num = 2;
lkb.list = lkblst;
lkb.num = 2;

init_blst(mnpcb->rlkb = &rlkb, MAX_LPDU_SZ, rlkbuff);
init_blst(mnpcb->lkb = &lkb, MAX_LPDU_SZ, lkbuff);

ftb.list = iatblst;
init_blst(mnpcb->ftb = &ftb,64+5, iatbuff);

/* Initialize async driver variables, too 
*/
p_mnpcb = mnpcb;
timerins();
drvr_ins();

}

/*LOCAL------------------------------------------------------------------------

	link_reset - 

-----------------------------------------------------------------------------*/

link_reset()
{

/* Disable interrupts while reseting the link.
*/
clr_int();

/* Reset lcb values - note that attention sequence numbers are not
** reintialized.
*/
lcb.lt_rsn = lcb.lt_ssn = lcb.ltrsn_acked = lcb.ltssn_acked = NULL;
lcb.lt_ret_cnt = lcb.lpdu_type = NULL;

lcb.status_1 &= ~(DATA_READY | ACK_TIMER | RET_TIMER);
lcb.status_2 &= ~(FORCE_ACK | FORCE_RET | ZERO_WNDW | WNDW_TIMER);
lcb.status_3 &= ~(DUP_IGNORED);

/* Reset_timers 
*/
ack_tm = fcw_tm = lr_tm = lt_tm  = NULL;

/* Reset send and receive framers 
*/
sf_busy = sf_lt = modem_out_busy = NULL;
rdle_flg = frame_snt = frame_rcvd = frame_dne =NULL;
sf_state = SF_INIT;
rf_state = RF_INIT;

/* Reset transmit and receive buffers 
*/
reset_blst(&rb);
reset_blst(&ftb);
reset_blst(&lkb);
reset_blst(&rlkb);

tbcnt = 0;

p_mnpcb->ld_reason = NULL;

/* Re-enable interrupts.
*/
set_int();

}

/*LOCAL------------------------------------------------------------------------

	lpdu_send - send an LPDU other than an LT

-----------------------------------------------------------------------------*/

SIGN_16 lpdu_send(type,wait)

SIGN_16 type,
	wait;
{

SIGN_16 retcode; 				/* return code */
struct BLST *snd_struct;			/* LPDU structure */

/* Get a buffer for the LPDU, send the LPDU, then release buffer. 
*/
get_b(&lkb,&snd_struct);
retcode = send_pdu(type,wait,snd_struct);
ret_b(&lkb,snd_struct);

/* Exit 
*/
return(retcode);
}

/*LOCAL------------------------------------------------------------------------

	parse_lr - parse an LR LPDU and perform parameter negotiation

-----------------------------------------------------------------------------*/

SIGN_16 parse_lr()

{

register USIGN_8 *p;			/* header pointer */
register USIGN_16 len,
			*pi;

/* Initialize 
*/
lcb.lr_parm = 0;
p = (rlkb.used_lst)->bptr;
len = *p++ - 2;

/* Process fixed fields 
*/
if (*p++ != LR)
	{
	ret_b(&rlkb,rlkb.used_lst);
	return(PROT_ERR);
	}

lcb.prot_level = min(lcb.prot_level,*p);
p++;

/* Process variable part 
*/
while (len > 0)
	{
	if (*p == 1)			/* skip parm 1 - serial no. */
		{
		len -= 8;
		p += 8;
		continue;
		}

	if (*p == 2)			/* take min of service class values */
		{
		len -= 3;
		p += 2;
		lcb.lr_parm |= LR_SRV_CLASS;
		lcb.srv_class = min(LCL_SCLASS, *p);
		p++;
		if (lcb.srv_class == 1)
			SETBIT1(HDUPLEX)
		continue;
		}

	if (*p == 3)			/* take min of window size */
		{
		len -= 3;
		p += 2;
		lcb.lr_parm |= LR_WNDW_SZ;
		lcb.window_sz = min (lcb.window_sz, *p);
		p++;
		continue;
		}

	if (*p == 4)
		{
		len -= 4;
		pi = (SIGN_16 *) (p += 2);
		lcb.lr_parm |= LR_DATA_SZ;
		lcb.max_data_sz = max (lcb.max_data_sz, *pi);
		p += 2;
		continue;
		}

	if (*p > 4)			/* ignore anything else */
		{
		len -= *++p + 2;
		p += *p++;
		break;
		}
	}

/* All done with LR in buffer, return the buffer 
*/
ret_b(&rlkb,rlkb.used_lst);

/* Now check for parms not received and set default values 
*/
if (!(lcb.lr_parm & LR_SRV_CLASS))
	lcb.srv_class = 1;

if (!(lcb.lr_parm & LR_WNDW_SZ))
	lcb.window_sz = 1;

if (!(lcb.lr_parm & LR_DATA_SZ))
	lcb.max_data_sz = BLK_DATA_SZ;

/* There is such a thing as a block-mode MNP link-connection, but
** this implementation will not talk to one (block mode links are
** for use with higher-level MNP protocols). 
*/
if (lcb.max_data_sz == BLK_DATA_SZ)
	return(BAD_LR_PARMS);

/* Adjust the credit for any receive buffers already used.
*/
lcb.lcl_credit = lcb.window_sz - rb.used;

/* Exit 
*/
return(SUCCESS);
}

/*LOCAL------------------------------------------------------------------------

	receive_wait -  wait for an LPDU during link establishment

-----------------------------------------------------------------------------*/

SIGN_16 receive_wait ()
{

/* Wait for an LPDU to be received or 2 seconds to go by, which ever
** occurs first.
*/
clr_int();
return(event_wait(20,FRAME_RCV));

}

/*LOCAL------------------------------------------------------------------------

	retran_lt - handle LT LPDU retransmission

-----------------------------------------------------------------------------*/

SIGN_16 retran_lt()
{

SIGN_16 i,
	retcode,
	not_done;
register USIGN_16 lts_unacked;
register struct BLST *snd_struct;

not_done = 1;
while (not_done)
	{

/* If there are no buffers in use, there is nothing to retransmit. 
*/
	if (ftb.used == 0)
		{
		CLRBIT2(FORCE_RET)			/* reset force flag */
		return(SUCCESS);			/* and just exit */
		}

/* Check for forced retransmission.  If so, clear flag and go right at
** it.  Otherwise, retransmit only when retran timer set and expired.
*/
	clr_int();
	if (BIT2SET(FORCE_RET))
		{
		CLRBIT2(FORCE_RET)
		set_int();
		}
	else
		{
		set_int();
		if (!BIT1SET(RET_TIMER))
	    		return(SUCCESS);
	if (lt_tm)
		return (SUCCESS);
	}

/* It's time to retransmit. But if we have retransmitted to the limit
** count, terminate the link.
*/
	if (lcb.lt_ret_cnt == RET_LIMIT)
		{
		as_disconnect(RETRAN_TMR_EXP,NULL);
		return(FAILURE);
		}

/* It's OK to still retransmit if there is remote credit (no need to
** check when window is only 1).
*/
	if (lcb.window_sz == 1)
		lcb.rem_credit = 0;
	else
		if (lcb.rem_credit == 0)
			return(SUCCESS);

/* Retransmit ALL unacknowledged LT's.
*/
	snd_struct = ftb.used_lst;
	not_done--;

	if (lcb.lt_ssn >= lcb.ltssn_acked)
		lts_unacked = lcb.lt_ssn - lcb.ltssn_acked;
	else
		lts_unacked = (256 - lcb.ltssn_acked) + lcb.lt_ssn;

	while (lts_unacked)
		{
		lts_unacked--;
		if (retcode = send_pdu(LT,NOWAIT,snd_struct))
			return(retcode);

		if (lcb.window_sz == 1) 
			event_wait(100,FRAME_SND);

		if (retcode = acking())
			return(retcode);
		if (lcb.prot_level == 2)
			{
	    		if (retcode = attn_process())
				return(retcode);
			}

		snd_struct = snd_struct->next_b;

/* If forced retransmission, i.e., LA received, we may have received
** another LA acknowledging frames that we would retransmit next.  If
** this happened, free up acked transmit buffer(s), then go back to
** beginning.  note: if there are buffers in use, say 3 & 4, & if after
** 3 & 4 are retransmitted, we receive an LA for 3, then this logic will
** retransmit 4 again. The last 4 will be ignored by receiver as an 
** immediate duplicate.
*/
		if (BIT2SET(FORCE_RET) || BIT1SET(LA_RECEIVED))
			{
			i = ftb.used;
			tb_free();
			if (i != ftb.used)
				{
/* Something has been freed.  If all buffers were freed, then we're
** all done.  If not, reset flags and do it over again.
*/
		    		if (ftb.used)
					{
					SETBIT2(FORCE_RET)
					not_done++;
					break;
					}
		    		}
			}
		}

	if (!not_done)
		{
		lcb.lt_ret_cnt++;
		lt_tm = lcb.lt_tmr;
		}
	}
	return(SUCCESS);
}

/*LOCAL------------------------------------------------------------------------

	send_la - send an LA LPDU

-----------------------------------------------------------------------------*/

SIGN_16 send_la()
{

SIGN_16 retcode;

/* Check for a zero window.  We want to remember sending a zero credit.
** We don't send zero credit, however, when the window is one. 
*/
if (!credit_chk())
	{
	SETBIT2(ZERO_WNDW)
	if (lcb.window_sz == 1)
		return(SUCCESS);
	}

/* Send an LA LPDU.  Return if link-connection is lost. 
*/
if (retcode = lpdu_send(LA,NOWAIT))
	return(retcode);

/* Reset ack and flow control timers. 
*/
clr_int();
CLRBIT1(ACK_TIMER)
ack_tm = NULL;
fcw_tm = lcb.window_tmr;
set_int();

/* Exit 
*/
return(SUCCESS);
}

/*LOCAL------------------------------------------------------------------------

	send_pdu - send an LPDU

-----------------------------------------------------------------------------*/

SIGN_16 send_pdu(type,wait,snd_struct)

SIGN_16 type,					/* LPDU type */
    wait;						/* wait for send to complete */
register struct BLST *snd_struct;
{

register USIGN_8 *p;
USIGN_8 *pli;
USIGN_16 *pi;
SIGN_16 i;

/* Just return if there is no physical connection. 
*/
if (lne_stat())
	return(NO_PHYSICAL);

/* If the lpdu is an LD, then we want to expedite it.  If an lpdu is being
** sent, wait a bit then truncate it.  This wait is necessary so that an
** empty frame (which is illegal) is not sent. 
*/
if (type == LD && sf_busy)
	{
	switch (lcb.baud_rate)
		{	
		case B_1200:
			i = 10;
			break;

		case B_300:
			i = 20;
			break;

		case B_110:
			i = 30;
			break;

		default:
			i = 5;
		}

	suspend(i);
	sf_len = 0;
    
	while (sf_busy)
		;
	}

/* Wait for the send framer to finish if a frame is in progress.  
*/
clr_int();
while (sf_busy)
	if (send_wait())
		return(TIME_OUT);

/* Now prepare the requested LPDU.
*/
p = snd_struct->bptr;

switch (type)
	{
	case LT:
		break;

	case LA:
		*p++ = LA_LEN;
		*p++ = LA;
		*p++ = 1;
		*p++ = 1;
		clr_int();
		*p++ = lcb.ltrsn_acked = lcb.lt_rsn;
		*p++ = 2;
		*p++ = 1;
		*p  = (USIGN_8) credit_chk();
		set_int();
		break;

	case LN:
		*p++ = LN_LEN;
		*p++ = LN;
		*p++ = 1;
		*p++ = 1;
		*p++ = lcb.ln_ssn;
		*p++ = 2;
		*p++ = 1;
		*p = lcb.ln_stype;
		break;

	case LNA:
		*p++ = LNA_LEN;
		*p++ = LNA;
		*p++ = 1;
		*p++ = 1;
		*p = lcb.ln_rsn;
		break;

	case LR:
		pli = p++;
		*p++ = LR;
		*p++ = lcb.prot_level;
		*p++ = 1;
		*p++ = 6;
		*p++ = 1;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 0;
		*p++ = 255;
		if (BIT1SET(MODE))
			{
	    		*pli = LR_ALEN;
			if (lcb.lr_parm & LR_SRV_CLASS)
				{
				*p++ = 2;
				*p++ = 1;
				*p++ = lcb.srv_class;
				*pli += 3;
				
				if (lcb.lr_parm & LR_WNDW_SZ)
					{
		    			*p++ = 3;
		    			*p++ = 1;
		    			*p++ = lcb.window_sz;
		    			*pli += 3;
		    
					if (lcb.lr_parm & LR_DATA_SZ)
						{
						*p++ = 4;
						*p++ = 2;
						pi = (USIGN_16 *) p;
						*pi = lcb.max_data_sz;
						*pli += 4;
						}
		    			}
				}
	    		}
		else
			{
			*pli = LR_ILEN;
			*p++ = 2;
			*p++ = 1;
			*p++ = LCL_SCLASS;
			*p++ = 3;
			*p++ = 1;
			*p++ = lcb.window_sz;
			*p++ = 4;
			*p++ = 2;
			pi = (USIGN_16 *) p;
			*pi = lcb.max_data_sz;
			}
		break;

	case LD:
		*p++ = LD_LEN;
		*p++ = LD;
		*p++ = 1;
		*p++ = 1;
		if ((*p = lcb.l_disc_code) == 255)
			{
			p++;
	    		*p++ = 2;
	    		*p++ = 1;
	    		*p = lcb.u_disc_code;
	    		*(snd_struct->bptr) += 3;
	    		}
		break;
	}

/* Pause a bit if this is a class 1 connection (hardly likely...)
*/
if (BIT1SET(HDUPLEX))
	suspend (20);

/* Set up the interrupt-driven send framer to send the lpdu.
*/
clr_int();
if (type == LT)
	sf_len = snd_struct->len;
else
	sf_len = *(snd_struct->bptr) + 1;

sf_ptr = snd_struct->bptr;
sf_busy = 1;
trigger_sf();
set_int();

/* Wait for the lpdu to get out, if that was what the caller wanted.
*/
if (wait)
	if (send_wait()) 
		return(TIME_OUT);

/* Exit
*/
return(SUCCESS);
}

/*LOCAL------------------------------------------------------------------------

	send_wait - wait for transmission of an LPDU or timeout

-----------------------------------------------------------------------------*/

SIGN_16 send_wait()
{

/* Wait 55 seconds (arbitrary number which is larger than the time
** needed to send a long LPDU), or until the LPDU is sent. If the time
** expires and frame send has not completed, return time out.  This
** means, effectively, that the hardware is broken.
*/
clr_int();
return(event_wait(550,FRAME_SND));

}

/*LOCAL------------------------------------------------------------------------

	tb_free - process acknowledged LT LPDUs

-----------------------------------------------------------------------------*/

tb_free()
{

register struct BLST *bp;
register USIGN_16 acked_lts;	    /* to hold value between 0 and 65535 */
register USIGN_16 head_ssn;

/* If there are no LT LPDUs unacknowledged, just clear force retransmission
** flag and return. 
*/
if (!tbcnt)
	{
	CLRBIT2(FORCE_RET)
	return;
	}

/* Some LT LPDU's have been sent.  However, only try to free up transmit
** buffers if an LA has been received. 
*/
clr_int();
if (BIT1SET(LA_RECEIVED))	
	{
	CLRBIT1(LA_RECEIVED)
	set_int();

/* Start freeing at the head of the list of buffers in use.  Free until
** the buffer just freed contains the LT with the same seq number as
** that of the last LT positively acked 
*/
	bp = ftb.used_lst;

	head_ssn = (USIGN_8)*(bp->bptr + LT_SEQ);

	if (head_ssn <= lcb.ltssn_acked)
		acked_lts = lcb.ltssn_acked - head_ssn +1;
	else
		acked_lts = (256 - head_ssn) + lcb.ltssn_acked + 1;

	while (acked_lts)
		{
		ret_b(&ftb,bp);
		bp = bp->next_b;
		acked_lts--;
		tbcnt--;
		}
	if (tbcnt < 0)
		tbcnt=0;

/* Clear force retransmission flag 
*/
	CLRBIT2(FORCE_RET)

/* If any LTs remain outstanding, reset the retransmission timer for them.
** Otherwise, be sure that the timer is cancelled. Since something has been
** acknowledged, the retransmission count can also be reset to 0. 
*/
	if (tbcnt)
		{
		SETBIT1(RET_TIMER)
		lt_tm = lcb.lt_tmr;
		ftb.used_lst = bp;
		}
	else
		{
		CLRBIT1(RET_TIMER)
		lt_tm = 0;
		}
	lcb.lt_ret_cnt = 0;
	}
else
	{
	set_int();
    	}
}
