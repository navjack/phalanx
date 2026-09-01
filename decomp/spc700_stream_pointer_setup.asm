; Stream-pointer/mode setup at SPC ARAM $09F0.
; Every Sound Test capture reaches this helper. It calls the shared `$159C`
; service, derives `$40/$41` from the `$15FE/$15FF` table, and establishes the
; associated mode state before returning.

SpcSetupStreamPointer:
	CALL $159C              ; 3F 9C 15
	MOV $08,A               ; C4 08
	MOV $04,A               ; C4 04
	ASL A                   ; 1C
	MOV X,A                 ; 5D
	MOV A,$15FF+X           ; F5 FF 15
	MOV Y,A                 ; FD
	MOV A,$15FE+X           ; F5 FE 15
	MOVW $40,YA             ; DA 40
	MOV $0C,#$02            ; 8F 02 0C
	MOV A,$1A               ; E4 1A
	EOR A,#$FF              ; 48 FF
	AND A,#$3F              ; 28 3F
	TSET $0046,A            ; 0E 46 00
	RET                     ; 6F
