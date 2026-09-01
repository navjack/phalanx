; Sequence-flow continuation at SPC ARAM $13ED.
; It decrements the event delay or loads/reloads stream pointer pairs, then
; returns to the `$13CB` stream-byte scanner.

SpcSequenceFlow:
	MOV A,$17               ; E4 17
	BEQ $1414               ; F0 23
	DEC $17                 ; 8B 17
	BNE $13FF               ; D0 0A
	MOV A,$0231+X           ; F5 31 02
	PUSH A                  ; 2D
	MOV A,$0230+X           ; F5 30 02
	POP Y                   ; EE
	BRA $13C7               ; 2F C8
	MOV A,$0241+X           ; F5 41 02
	PUSH A                  ; 2D
	MOV A,$0240+X           ; F5 40 02
	POP Y                   ; EE
	BRA $13C7               ; 2F BE
	INC Y                   ; FC
	MOV A,($14)+Y           ; F7 14
	PUSH A                  ; 2D
	INC Y                   ; FC
	MOV A,($14)+Y           ; F7 14
	MOV Y,A                 ; FD
	POP A                   ; AE
	BRA $13C7               ; 2F B3
