; E0 instrument descriptor-pointer setup at SPC ARAM $1044.
; The selected program indexes a six-byte descriptor and chooses a voice slot.

SpcSequenceE0Descriptor:
	MOV Y,#$06              ; 8D 06
	MUL YA                  ; CF
	MOVW $14,YA             ; DA 14
	CLRC                    ; 60
	ADC $14,#$00            ; 98 00 14
	ADC $15,#$3E            ; 98 3E 15
	MOV A,$1A               ; E4 1A
	AND A,$47               ; 24 47
	BNE $1090               ; D0 3A
	PUSH X                  ; 4D
	MOV A,X                 ; 7D
	XCN A                   ; 9F
	LSR A                   ; 5C
	OR A,#$04               ; 08 04
	MOV X,A                 ; 5D
