	title mnpc\setpar.asm
	page	60,132
;==============================================================================
;
;		              The Microcom MNP Library
;	          	      (Microsoft C Version)
;
;------------------------------------------------------------------------------
;
;	Setpar - set input data byte to specified character format
;
;	Synopsis:  c = setpar(data_byte, par_type);
;
;			char data_byte;
;			int par_type;
;
;==============================================================================

_data	segment word public 'DATA'
_data	ends
dgroup	group _data

_text	segment byte public 'CODE'
	assume 	cs:_text,ds:dgroup

	public	_setpar

; Local equates
NONE	=	0
EVEN	=	1
ODD	=	2
MARK	=	3
SPACE	=	4
;
_setpar	proc	near
	push	bp				;save register
	mov	bp,sp			;point to parameters

	mov	ah,[bp+6]			;get parity type
	mov	al,[bp+4]			;get data byte

	cmp	ah,NONE 			;8 none?
	je	setp9			;yes-go exit

	and	al,07fh 			;clear parity bit

	cmp	ah,EVEN 			;7 even?
	jne	setp1			;no-go on

	or	al,al			;get current parity sense
	jpe	setp9			;if even, go exit
	or	al,80h			;else set parity bit
	jmp	short setp9		;and go exit

setp1:
	cmp	ah,ODD			;7 odd?
	jne	setp2			;no-go on

	or	al,al			;get current parity sense
	jpo	setp9			;if odd, go exit
	or	al,80h			;else set parity bit
	jmp	short setp9		;and go exit

setp2:
	cmp	ah,MARK 			;7 mark?
	jne	setp9			;no-already set to space, go exit

	or	al,80h			;set parity bit

setp9:
	xor	ah,ah			;clear high part of ax
	pop	bp				;restore register
	ret					;return byte in ax

_setpar	endp

_text	ends
	end
