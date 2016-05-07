	title	timers.. timerins, timerrem, and timerint
	page	60,132
;==============================================================================
;
;		     	  The Microcom MNP Library
;		 		   (Microsoft C Version)
; 
;------------------------------------------------------------------------------
;
;	timerins -- timer install routine
;
;	synopsis: timerins();
;
;------------------------------------------------------------------------------
;
;	timerrem -- timer remove routine
;
;	synopsis: timerrem();
;
;------------------------------------------------------------------------------
;
;    timerint -- timer interrupt routine (invoked on timer tick)
;
;------------------------------------------------------------------------------
;
;	Modification History
;
;	10/29/87 - timerint allowed to run with interrupts enabled..gp
;
;==============================================================================

_data	segment word public 'DATA'
_data   ends
dgroup  group	_data

_data   segment
old_t_vct	label	dword		; old timer vector contents
old_bx		dw	0		; base
old_es		dw	0		; segment
timer_active	db	0		; timer active flag
tick		db	18		; clock tick count

timer_no  equ	6			; number of timers in table

	extrn	_g_tm:word	; start of timers 

_data	ends

_text	segment byte public 'CODE'
	assume	cs:_text,ds:dgroup

	public	_timerins,_timerrem
	public	cdsg
	public 	_timer_int


; Data in code segment
cdsg	dw	0

	page
;PUBLIC***********************************************************************
;
;	timerins - timer installation routine
;
;*****************************************************************************

_timerins proc near

	push	bp
	push	es				; save es and ds
	push	ds

; Check to be sure that the timer interrupt handler is not already
; installed.  Do nothing in this case.
	cmp	timer_active,1		; already installed?
	je	tin9				; yes-go return

; Initialize
	mov	timer_active,1		; set timer active flag
	mov	cs:cdsg,ds		; save data segment

; Save current interrupt vector contents then install our own handler.
	mov	ah,35h			; get vector request
	mov	al,1ch			; we want the timer one
	int	21h				; int to DOS
	mov	old_bx,bx			; save vector contents
	mov	old_es,es
	mov	dx,offset _timer_int	; point to our handler
	mov	ax,cs			; be sure ds is right
	mov	ds,ax
	mov	ah,25h			; set vector request
	mov	al,1ch			; we want the timer one
	int	21h				; int to DOS

; Return
tin9:
	pop    ds				; restore data segment
	pop    es				; restore es
	pop	bp
	ret

_timerins endp
 	page
;PUBLIC***********************************************************************
;
;	timerrem - timer removal subroutine
;
;*****************************************************************************

_timerrem proc near

	push	bp
; Check to be sure that the timer interrupt handler is already
; installed.  Do nothing if not.
	cmp	timer_active,1		; installed?
	jne	tir9				; no-go return

; Recover old vector contents and put them back.
	push	ds				; save ds register
	mov	ah,25h			; set vector request
	mov	al,1ch			; we want the timer one
	mov	dx,old_bx			; recover old vector
	mov	ds,old_es
	int	21h				; int to DOS
	pop	ds				; restore ds register
	mov	timer_active,0		; reset timer active flag

; Return
tir9:
	pop	bp
	ret

_timerrem endp
	page
;*****************************************************************************
;
;	timerint - timer interrupt routine
;
;*****************************************************************************

_timer_int proc far

; Initialize
	push	ds				; save registers
	mov	ds,cs:cdsg		; enable data addressability

; Re-enable interrupts
;####	sti

; When a clock interrupt occurs, decrement the second ticker.  If the
; ticker goes to zero, count down the second-based timers.
t1:
	dec	tick				; decrement ticker
	jnz	tiret			; go on if not zero

; Pass through second-based timers and decrement any that are not already
; zero.  Zero can mean either 'elapsed' or 'inactive'.
ti1:
	mov	tick,18 			; reset ticker

	push	bx				; save registers
	push	cx
	mov 	bx,offset _g_tm
;	mov	bx,offset timers	; point to list of timers
	mov	cx,timer_no		; do for each timer

ti3:
	cmp	word ptr [bx],0 	; active?
	je	ti4				; no-go on
	dec	word ptr [bx]		; yes-count it down

ti4:
	add	bx,2				; advance to next timer
	loop	ti3				; and try again

	pop	cx				; restore registers
	pop	bx				; and go on

tiret:
	cli					;  disable interrupts before call
	pushf				; this is to simulate an int instruction
	call	old_t_vct			; inter-segment indirect call

	pop	ds
	sti					; re-enable interrupts
	iret					; and return from interrupt

_timer_int endp

_text	ends
	end
