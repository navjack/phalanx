; Derived vibrato-value helper at SPC ARAM $10CF.
; It stores the calculated modulation value for the later `$143B` service.

SpcStoreVibratoDerivedValue:
	MOV $02B1+X,A           ; D5 B1 02
	PUSH A                  ; 2D
	MOV Y,#$00              ; 8D 00
	MOV A,$B1+X             ; F4 B1
	POP X                   ; CE
	DIV YA,X                ; 9E
	MOV X,$44               ; F8 44
	MOV $02C0+X,A           ; D5 C0 02
	RET                     ; 6F
