	title		async.asm
	page	60,132
;==============================================================================
;
;			       The Microcom MNP Library
;					(Microsoft C Version)
;
;------------------------------------------------------------------------------
;
;		ASYNC - Async I/O interrupt handler management routines
;				(set/clear interrupt routines also here)
;
;---------------------------------------------------------------------------
;
;	Modification History:
;
;	4/1/87 - Compuserve V1.0
;
;==============================================================================
;
ic_imr	equ	21h			;i/o address of interrupt mask reg

; Public declarations
	public	_drvr_ins
	public	_drvr_rem
	public	_clr_int
	public	_set_int
;	public	_port_add
	extrn	_port_add:word
;	public	_hard_int
;	public	_chan_no
	extrn	_chan_no:word

; Externals
	extrn	_ascode:near

; Data Segment
_data	segment word public 'DATA'
_data	ends
dgroup	group	_data
_data	segment word public 'DATA'
	assume	ds:dgroup

;	public	_linestat
	extrn	_linestat:byte
;	public	_iir_add
	extrn	_iir_add:word
	public	_async

	extrn	_baudrate:byte
	
_hard_int	db	0			;hardware interrupt #
;_port_add dw	0				;base address of sio hardware
;_chan_no dw	0				;channel # being used
;
	db	100 dup(?)
stack	equ	$
drvr_active	db	0
;_linestat	db	?
old_vct 	dw	?
		dw	?
;_iir_add 	dw	?
ic_imr_sav	db	?
;
_data	ends

_text	segment byte public 'CODE'
	assume	cs:_text,ds:dgroup

	extrn	cdsg:word

	page
;PUBLIC---------------------------------------------------------------------
;
;	_drvr_ins - install async interrupt handler and set up async card
;
;---------------------------------------------------------------------------

_drvr_ins	proc	near
		
	push	bp
	mov	bp,sp

	push	bx
	push	dx
	push	es

	cmp	drvr_active,1		;already installed ?
	jne	doit				;no, go install
	jmp	done

doit:
	mov	ax,_chan_no		; get channel number
	mov	_port_add,03f8h	; assume port 1
	mov	_hard_int,0ch
	cmp	al,0				; is it 1?
	je	do1				; yes-go on
	mov	_port_add,02f8h	; no-setup for port 2
	dec	_hard_int
do1:
	in	al,ic_imr
	jmp	short $+2
	mov	ic_imr_sav,al
	mov	al,0ffh
	out	ic_imr,al

	mov	ah,35h			;ask for current vector contents
	mov	al,_hard_int		;specify which vector
	int	21h

	mov	old_vct,bx		;save current vector values
	mov	old_vct+2,es

	mov	dx,offset _async
	mov	ah,25h			;install new vector values
	mov	al,_hard_int
	push	ds				;save current ds
	push	cs
	pop	ds				;ds = cs
	int	21h
	pop	ds				;restore ds

	mov	ax,_port_add
	inc	ax
	inc	ax
	mov	_iir_add,ax
;
	mov	drvr_active,1
;
;      initialize rs-232 driver to character format and baud rate
;
	mov	al,_baudrate
	mov	cl,5				;adjust for format
	shl	al,cl			;move  into correct field
	or	al,03			;set up 8 bits, no parity, 1 stop bit
	xor	ah,ah			;rom bios control code
	mov	dx,_chan_no		;point to port number
	int	14h				;rom bios interrupt
;
;	set current modem status
;
	mov	dx,_port_add
	add	dx,6				;point to modem status register
	in	al,dx
	jmp	$+2
	nop
	in	al,dx			;do it again in case change was waiting
	mov	_linestat,al
;
;	enable modem status, rcvr and xmtr interrupts
;
	sub	dx,5				;point to interrupt enable register
	mov	al,00001011b
	out	dx,al
	jmp	$+2
	nop
;
;	clear pending interrupts
;
	inc	dx	       		;point to interrupt identification reg
	in	al,dx	       	;clear xmtr interrupt by reading iir
	jmp	$+2
	nop
	sub	dx,2	       		;point to receive data buffer
	in	al,dx	       	;clear rcvr interrupt by reading rxb
	jmp	$+2
	nop
;
;	setup dtr, rts & interrupt hardware enable
;
	add	dx,4	       		;point to modem control register
	mov	al,00001011b  		 ;enable dtr, rts and interrupt (out 2)
	out	dx,al
	jmp	$+2
	nop
;
;      enable interrupt in controller.
;      if the port address is 2f8, use interrupt 3; otherwise use int 4.
;
		mov	al,ic_imr_sav
		mov	ah,0ffh - 00010000b	;assume int 4
		cmp	_port_add,02f8h		;com2?
		jne	port1			;no-go on
		mov	ah,0ffh - 00001000b	;switch to int 3
port1:		
		and	al,ah			;enable irq
		out	ic_imr,al
		jmp	$+2
		nop
;
done:
		pop	es
		pop	dx
		pop	bx
		pop	bp
		ret
;
_drvr_ins	endp
		page
;PUBLIC---------------------------------------------------------------------
;
;	_drvr_rem - remove async interrupt handler
;
;--------------------------------------------------------------------------

_drvr_rem	proc	near
		push	bp
		mov	bp,sp
;
		push	dx
;
		cmp	drvr_active,1
		jne	rdone
;
;	reinstall system interrupt handler
;
		push	ds		    ;save current ds
		mov	ah,25h
		mov	al,_hard_int
		mov	dx,old_vct
		mov	ds,old_vct+2
		int	21h
		pop	ds		    ;restore ds
;
		mov	drvr_active,0
rdone:
		pop	dx
		pop	bp
		ret
;
_drvr_rem	endp

		page
;LOCAL----------------------------------------------------------------------
;
;	_async - async interrupt handler
;
;---------------------------------------------------------------------------

_async	proc	far
	
		push	bp
		push	ax
		push	bx
		push	cx
		push	dx
		push	si
		push	di
		push	ds
		push	es
;
		mov	di,ss
		mov	si,sp
;
		mov	ax,cs:cdsg
		mov	ss,ax
		mov	es,ax
		mov	ds,ax
;
		mov	ax,offset dgroup:stack
		mov	sp,ax
		mov	bp,ax
;
		push	di
		push	si
;
		cld
;
		call	_ascode
;
		pop	si
		pop	di
		mov	ss,di
		mov	sp,si
;
		pop	es
		pop	ds
		pop	di
		pop	si
		pop	dx
		pop	cx
		pop	bx
		pop	ax
		pop	bp
		sti
		iret
;
_async		endp
		page
;PUBLIC---------------------------------------------------------------------
;
;	_clr_int - do a cli from C
;
;---------------------------------------------------------------------------

_clr_int 	proc	near
		cli
		ret
_clr_int 	endp

;PUBLIC---------------------------------------------------------------------
;
;	_set_int - do an sti from C
;
;---------------------------------------------------------------------------

_set_int 	proc	near
		sti
		ret
_set_int 	endp

_text ends

	end
