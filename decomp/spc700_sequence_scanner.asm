; Live sequence byte scanner at SPC ARAM $13C7.
; $13CB is the traced F7 reader responsible for every reported stream byte.

SpcReadSequenceByte:
	MOVW $14,YA             ; DA 14
	MOV Y,#$00              ; 8D 00
SpcReadSequenceByteNext:
	MOV A,($14)+Y           ; F7 14
	BEQ $13ED               ; F0 1E
	BMI $13D8               ; 30 07
	INC Y                   ; FC
	BMI $1414               ; 30 40
	MOV A,($14)+Y           ; F7 14
	BPL $13D1               ; 10 F9
	CMP A,#$C8              ; 68 C8
	BEQ $141B               ; F0 3F
	CMP A,#$EF              ; 68 EF
	BEQ $1409               ; F0 29
	CMP A,#$E0              ; 68 E0
	BCC $1414               ; 90 30
	PUSH Y                  ; 6D: preserve current stream offset.
	MOV Y,A                 ; FD: control byte indexes the advance table.
	POP A                   ; AE: recover the original stream offset.
	ADC A,$11FC+Y           ; 96 FC 11: advance by this control's width.
	MOV Y,A                 ; FD
	BRA SpcReadSequenceByteNext ; 2F DE
