/*==========================================================================

       		        The Microcom MNP Library
					(Microsoft C Version)

----------------------------------------------------------------------------

	            MNPINTRF - MNP Link Interface Routines

----------------------------------------------------------------------------

	Modification History

	3/30/87 - Compuserve V1.0

==========================================================================*/

/* Header files
*/
#include <dos.h>
#include <mnpdat.h>

/* Local definitions
*/
#define NOWAIT		0
#define BIT3SET(bit)		(lcb.status_3 & bit)
#define BIT1SET(bit)		(lcb.status_1 & bit)
#define SETBIT1(bit)		lcb.status_1 |= bit;
#define SETBIT3(bit)		lcb.status_3 |= bit;

USIGN_8 get_sbuf();
void mnpdisconnect();
SIGN_16 ia_send();

/* Link status block
*/
struct link_stat_blk
	{
	USIGN_16 p_status;
	USIGN_16 l_status;
	USIGN_16 s_count;
	USIGN_16 r_count;
	USIGN_16 all_sent;
	};

/* External references
*/
extern struct BUFFER ftb;
extern struct link_ctl_blk lcb;	/* link control block */
extern USIGN_16 sf_busy,				/* 'send framer busy' flag */
	sf_lt,					/* 'LT in progress' flag */
	sf_len,					/* send framer length */
	tbcnt,					/* unacked transmit buf count */
	ln_tm,					/* LN retransmission timer */
	lt_tm;					/* LT retransmission timer */
extern USIGN_8 chan_no;			/* hardware channel no. */

/* Data declarations
*/
struct MNP_CB mnpcb;			/* MNP control block */

USIGN_8 rbuf[RBUF_LEN];			/* receive buffer */
USIGN_8 *rb_iptr,				/* rcv insert ptr */
	*rb_rptr; 				/* rcv remove ptrs */
USIGN_16 rb_cnt;				/* rcv count */

USIGN_8 sbuf[SBUF_LEN];			/* send buffer */
USIGN_8 *sb_iptr,				/* snd insert ptr */
	*sb_rptr;					/* snd remove ptr */
USIGN_16 sb_cnt;				/* snd count */

USIGN_8 *iatbiptr;				/* rcv temp insert ptr */
struct BLST *iatb_struct;		/* transmit buffers */

USIGN_8 baudrate;				/* line speed */

/*GLOBAL***********************************************************************

	mnpconnect - establish an MNP link-connection

	synopsis:	retcode = mnpconnect(rate,format,port,mode);

	input:	int rate,		physical-connection speed 
			    format, 	character format
			    port,		comm port
			    mode;		originator/answerer

	output:	int retcode;	establishment result

**************************************************************************/

SIGN_16 mnpconnect(rate,format,port,mode)

USIGN_16 rate,
    format,
    port,
    mode;
{

/* Initialize send, receive counts and buffer pointers.
*/
rb_cnt = sb_cnt = 0;
rb_iptr = rb_rptr = rbuf;
sb_iptr = sb_rptr = sbuf;

/* Initialize physical-connection variables
*/
baudrate = rate;
chan_no = port-1;
mnpcb.parity = format;

/* Attempt link establishment and exit.  as_connect return code is
** returned as function value.
*/
return(as_connect(&mnpcb,mode));

}

/*GLOBAL***********************************************************************

	mnpdisconnect - terminate an MNP link-connection

	synopsis:	mnpdisconnect();

	input:	none

	output:	none
		       ******************************************************************************/
void mnpdisconnect()
{

/* Just call mnpllvl routine, signalling link-user initiated termination.
*/
as_disconnect(255,0);

}


/*GLOBAL***********************************************************************

	mnpreceive - receive data on an MNP link-connection

	synopsis:	retcode = mnpreceive(&buffer,buflen);

	input:	unsigned char *buffer;	pointer to user buffer
			int buflen;			length of user buffer

	output:	int retcode;	if positive, the number of data bytes
						copied into the user data buffer;
						if negative, link error code

******************************************************************************/

SIGN_16 mnpreceive(bufptr,buflen)

USIGN_8 *bufptr;
USIGN_16 buflen;
{

SIGN_16 retcode;
register USIGN_16 count;
register USIGN_8 c;

/* Maintain the link-connection.  On link error, return with the 
** error code as the function value.
*/
if (retcode = as_link())
	return(retcode);
if (retcode = ia_send(&mnpcb))
	return(retcode);

/* Initialize
*/
count = 0;

/* If there is receive data, copy as much as possible into the
** user's buffer.  Note that if a 7-bit character format has been
** selected for the link, then the high-order bit of the data octet
** is set to 0.
*/
while ((rb_cnt > 0) && (buflen > 0))
	{
     buflen--;
     count++;
     rb_cnt--;
     c = *rb_rptr++;
     if (mnpcb.parity) c &= 0x7f;
     *bufptr++ = c;
     if (rb_rptr >= (rbuf + RBUF_LEN))
		rb_rptr = rbuf;
     }

/* Maintain the link-connection.  On link error, return with error
** code as function value.
*/
if (retcode = as_link())
	return(retcode);
if (retcode = ia_send(&mnpcb))
	return (retcode);

/* Exit.  Function value is the number of octets moved into the user
** buffer.
*/
return(count);
}

/*GLOBAL***********************************************************************

	mnpsend - send data on an MNP link-connection

	synposis:	retcode = mnpsend(&buffer,buflen);

	input:	unsigned char *buffer; 	pointer to user data buffer
	     	int buflen;			length of user data buffer

	output:   int retcode - if positive, the number of data bytes
		                   copied from the user data buffer; 
                             if negative, link error code

***************************************************************************/

SIGN_16 mnpsend(bufptr,buflen)

USIGN_8 *bufptr;
USIGN_16 buflen;
{

SIGN_16 retcode;
register USIGN_16 count;

/* Maintain the link-connection.
*/
if (retcode = as_link())
	return(retcode);

/* Initialize
*/
count = 0;

/* Copy as much user data as possible into the send ring buffer.
*/
while ((sb_cnt < SBUF_LEN) && (buflen > 0))
	{
	count++;
	sb_cnt++;
	buflen--;
	*sb_iptr++ = (unsigned char) setpar(*bufptr++,mnpcb.parity);
	if (sb_iptr >= sbuf + SBUF_LEN)
		sb_iptr = sbuf;
	}

/* Try to send an LT LPDU or continue one in progress and maintain
** the link once again.
*/
if (retcode = ia_send(&mnpcb))
	return(retcode);
if (retcode = as_link())
	return(retcode);

/* Exit
*/
return(count);
}

/*GLOBAL***********************************************************************

	mnpstatus - get link-connection status information

	synopsis:	mnpstatus(&lstat_blk);

	input:	struct link_stat_blk *lstat_blk;	pointer to status block

	output:	none

******************************************************************************/

mnpstatus(lsb)

struct link_stat_blk *lsb;
{

/* Maintain the link-connection.  Here ignore any link errors as these
** will be reflected in status values.
*/
as_link();
ia_send(&mnpcb);

/* Build link status block.
*/
if (lne_stat())
    lsb->p_status = FALSE;
else
    lsb->p_status = TRUE;

if (BIT1SET(LINK_EST))
    lsb->l_status = TRUE;
else
    lsb->l_status = FALSE;

/* Number of bytes free in send buffer. By definition this number is 
** the size of the send buffer when the link-connection is not 
** established.
*/
if (lsb->l_status == FALSE)
	sb_cnt = 0;
lsb->s_count  = SBUF_LEN - sb_cnt;

/* Amount of data in receive buffer
*/
lsb->r_count = rb_cnt;

/* Data is 'all sent' if the user send buffer is empty and all data
** messages have been acknowledged.
*/
if ((sb_cnt == 0) && (tbcnt == 0))
	lsb->all_sent = TRUE;
else
	lsb->all_sent = FALSE;

}

/*GLOBAL********************************************************************

	mnpbreak - send a break signal (using MNP attention)

	synopsis:	retcode = mnpbreak();

	input:	none

	output:	int retcode;	link error code

***************************************************************************/

SIGN_16 mnpbreak()

{
SIGN_16 retcode;

/* If attention service has not been negotiated for this link-connection,
** return.  Function value = attention not supported.
*/
if (lcb.prot_level != 2)
	return(-75);

/* Maintain the link.  Return link error code if link has failed.
*/
if (retcode = as_link())
	return(retcode);

/* If an attention has already been sent and not yet acknowledged, just
** return.  Funtion value = attention in progress.  
*/
if (BIT3SET(LN_SENT))
	return(-76);

/* Ok to send a destructive break using the MNP attention mechanism.
*/
clr_int();
rb_iptr = rb_rptr = rbuf;
sb_iptr = sb_rptr = sbuf;
rb_cnt = sb_cnt = NULL;

SETBIT3(LN_SENT)

link_reset();
dphase_init();

SETBIT3(LN_TIMER)
ln_tm = lcb.lt_tmr;

++lcb.ln_ssn;
lcb.ln_stype=1;
set_int();
return(lpdu_send(LN,NOWAIT));
}

/*LOCAL------------------------------------------------------------------------

	ia_send - build an LT or add data to one in progress

-----------------------------------------------------------------------------*/

SIGN_16 ia_send(mnpcb)

struct MNP_CB *mnpcb;
{

SIGN_16 retcode;

/* Just exit if there is nothing to send.
*/
if (sb_cnt == 0)
	return(SUCCESS);

/* Also just return if an attention is in progress.
*/
if (BIT3SET(LN_RECEIVED) || BIT3SET(LN_SENT))
	return(SUCCESS);

/* If the send framer is not busy and there is remote credit,
** start an LT LPDU.
*/
clr_int();
if (sf_busy != 1)
	{
	set_int();
	if (lcb.rem_credit == 0)
		return(SUCCESS);
	if (get_b(mnpcb->ftb, &iatb_struct) == FAILURE)
		return(SUCCESS);

	iatb_struct->len = 5;
	iatbiptr = iatb_struct->bptr;
	*iatbiptr++ = 4;
	*iatbiptr++ = LT;
	*iatbiptr++ = 1;
	*iatbiptr++ = 1;
	*iatbiptr++ = ++lcb.lt_ssn;

/* Copy bytes out of the send buffer as long as there are any and there
** is space in the LT LPDU (max is 64 + header of 5 = 69 for Class 3)
*/
	while ((iatb_struct->len < 69) && (sb_cnt > 0))
		{
		iatb_struct->len++;
		*iatbiptr++ = get_sbuf();
		}

/* Adjust remote credit.
*/
	clr_int();
	if (--lcb.rem_credit < 0)
		 lcb.rem_credit = 0;
	tbcnt++;
	set_int();

/* Now start sending an LT LPDU.  Initiate the LT retransmission timer
** if it is not already active.
*/
	if (retcode = send_pdu(LT, NOWAIT, iatb_struct))
		return(retcode);
	sf_lt = 1;

	if (!BIT1SET(RET_TIMER))
		{
		SETBIT1(RET_TIMER)
		lt_tm = lcb.lt_tmr;
		}

	if (ftb.used == 1)
		ftb.used_lst = iatb_struct;
	}

/* If the send framer is busy and it is busy with an LT LPDU, try to add
** as much send buffer data as possible to the current LT.
*/
else
	{
	if (sf_lt == 1)
		{
		while ((iatb_struct->len < 69) && (sb_cnt > 0))
			{
			iatb_struct->len++;
			sf_len++;
			*iatbiptr++ = get_sbuf();
			}
		}
	}

/* Be sure that interrupts are re-enabled.
*/
set_int();

/* Exit
*/
return(SUCCESS);
}

/*LOCAL------------------------------------------------------------------------

	get_sbuf - get a data octet from the send buffer

-----------------------------------------------------------------------------*/

USIGN_8 get_sbuf()
{

register USIGN_8 c;

/* Remove an octet from the send buffer and return as function value.
** This routine handles the buffer count and pointer into the circular
** buffer.
*/
sb_cnt--;	
c = *sb_rptr++;
if (sb_rptr >= sbuf + SBUF_LEN)
	sb_rptr = sbuf;

/* Exit
*/
return(c);
}
