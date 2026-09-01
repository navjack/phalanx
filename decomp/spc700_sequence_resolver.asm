; Control-effect resolver at SPC ARAM $101C.
; A control byte selects a handler return target and stream-width entry.

SpcResolveSequenceControl:
	ASL A                   ; 1C
	MOV Y,A                 ; FD
	MOV A,$11DF+Y           ; F6 DF 11: handler target high byte.
	PUSH A                  ; 2D
	MOV A,$11DE+Y           ; F6 DE 11: handler target low byte.
	PUSH A                  ; 2D
	MOV A,Y                 ; DD
	LSR A                   ; 5C
	MOV Y,A                 ; FD
	MOV A,$127C+Y           ; F6 7C 12: stream-width lookup.
	BEQ $1036               ; F0 08
	MOV A,($30+X)           ; E7 30
	INC $30+X               ; BB 30
	BNE $1036               ; D0 02
	INC $31+X               ; BB 31
	MOV Y,A                 ; FD
	RET                     ; 6F
