	title	fcscalc.asm
	page	60,132
;==============================================================================
;
;			         The Microcom MNP Library
;					(Microsoft C Version)
;
;------------------------------------------------------------------------------
;
;	fcscalc - calculate frame check sequence
;
;	synopsis: 
;		
;			fcscalc(fcs_word,octet);
;
;	input:
;			struct {char low, hi;}  *fcs_word;
;			char octet;
;
;==============================================================================

_data 	segment word public 'DATA'
_data	ends
dgroup	group	_data

_data	segment 
temp	db	?			; temp variable
_data	ends

_text	segment byte public 'CODE'
	assume 	cs:_text,ds:dgroup

	public	_fcscalc

fcs	=	4
octet	=	6
hi	=	1
;
;
_fcscalc proc	near

	push	bp
	mov	bp,sp
;
	push	ax
	push	bx
	push	cx
	push	si
;
	mov	ax,[bp+octet]	;get octet
	mov	si,[bp+fcs]
	xor	al,[si+hi]	;hi byte of resulting fcs number
	mov	temp,al
	mov	bl,al
;
	mov	cx,7
lp1:	sar	bl,1
	add	al,bl
	loop	lp1
;
	and	al,01h
	mov	bl,temp
	sar	al,1
	rcr	bl,1
	rcr	al,1
	sar	bl,1
	rcl	bl,1
	pushf			;save carry flag
	xor	bl,temp

;	popf			;recover carry flag
	jmp	$+3			; simulate popf
	iret
	push	cs
	call	cs:$-2

	rcr	bl,1
	rcr	al,1
	sar	bl,1
	rcl	bl,1
	adc	al,00h
	xor	al,[si] 	;low byte of resulting fcs number
	mov	[si],bl 	;save new low byte
	mov	[si+hi],al	;save new hi byte
;
	pop	si
	pop	cx
	pop	bx
	pop	ax
;
	pop	bp
	ret
;
_fcscalc endp

_text	ends
	end
