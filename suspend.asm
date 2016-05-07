	TITLE suspend.asm ...suspend (n) 100 msec periods.
	page    60,132
;==============================================================================
;
;		       The Microcom MNP Library
;			(Microsoft C Version)
;
;------------------------------------------------------------------------------
;
;	suspend - wait n 100 msec periods
;
;	synopsis: suspend(n);
;
;			int n;		number of periods
;
;==============================================================================

_data	segment word public 'DATA'
_data	ends
dgroup	group	_data

_text	segment byte public 'CODE'
	assume	cs:_text,ds:dgroup

	public	_suspend

;PUBLIC****************************************************************
;
;	suspend - wait n 100ms periods
;
;**********************************************************************

_suspend proc near

	push	bp
	mov	bp,sp

	mov	al, byte ptr[bp+4]	; get time to suspend
;
w0:	
	push	ax
	mov	ah,2ch			;get time of day
	int	21h			;  from dos
	mov	bl,dl			;save hundredths value
	pop	ax
;
; loop until the time-of-day clock indicates that 10x.01 seconds
; have gone by.
w1:	
	push	ax			;save count
	mov	ah,2ch			;get time again
	int	21h
	cmp	dl,bl			;wrap?
	jge	w2			;no-go on
	add	dl,100			;yes-compensate
w2:	
	sub	dl,bl			;get difference between then and now
	cmp	dl,10			;has 100 ms gone by?
	pop	ax
	jl	w1			;no-go again
;
	dec	al			;count down
	jnz	w0			;loop until done
;
	pop	bp			;restore bp
	ret

_suspend endp

_text	ends
	end
