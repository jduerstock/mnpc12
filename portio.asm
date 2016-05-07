	title	portio.asm
	page	65,132
;==============================================================================
;
;	     	       The Microcom MNP Library
;		             (Microsoft C Version)
;
;---------------------------------------------------------------------------
;
; 			PORTIO - MNP port input/output routines
;
;------------------------------------------------------------------------------
;
;	Modification History:
;
;	4/1/87 - Compuserve V1.0
;
;---------------------------------------------------------------------------
;
;	inp - input byte from port
;
;	synopsis: c = inp(port);
;
;	input:
;			int port;		port address
;
;	output:
;			int c:		returned data byte
;
;------------------------------------------------------------------------------
;
;	outp - output byte to port
;
;	synopsis: outp(port,c);
;
;			int port;	port address
;			int c;		byte to send
;
;==============================================================================
;
_data	segment word public 'DATA'
_data	ends
dgroup	group	_data

_text	segment byte public 'CODE'
	assume	cs:_text,ds:dgroup

	public	_inp
	public	_outp

;PUBLIC************************************************************************
;
;	_inp - read port
;
;******************************************************************************

_inp	proc	near

	push	bp
	mov	bp,sp

	mov	dx,[bp+4]
	in	al,dx
	xor	ah,ah

	POP	BP
	RET

_inp	endp
	page
;PUBLIC************************************************************************
;
;	_outp - write port
;
;******************************************************************************

_outp	proc	near

	push	bp
	mov	bp,sp

	mov	dx,[bp+4]
	mov	ax,[bp+6]
	out	dx,al

	pop	bp
	ret

_outp	endp

_text	ends
	end
