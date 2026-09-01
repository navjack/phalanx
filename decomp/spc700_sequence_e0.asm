; Sequence control E0: instrument/program selection, traced at $1038.
; The downstream DSP trace proves this state drives SRCN and ADSR writes.

SpcSequenceE0Instrument:
	MOV $0211+X,A           ; D5 11 02
	MOV Y,A                 ; FD
	BPL $1044               ; 10 06
	SETC                    ; 80
	SBC A,#$CA              ; A8 CA
	CLRC                    ; 60
	ADC A,$5F               ; 84 5F
