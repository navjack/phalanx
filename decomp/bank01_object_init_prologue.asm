; Enter the object-state initializer with a 16-bit accumulator/index pair.
; Direct-page state is cleared before the routine switches back to byte mode.

ObjectInit:
	REP #$20
	LDA.w #$0000
	TCD
	PHA
	REP #$10
	STZ $E3
	STZ $E5
	STZ $E7
	STZ $E9
	STZ $EA
	STZ $EC
	STZ $EE
	STZ $F0
	STZ $F2
	STZ $F4
	STZ $F6
	STZ $F8
	STZ $FA
	STZ $FC
	STZ $FE
	SEP #$20
	PHA
	LDA.b #$00
	XBA
	PLA
	LDX.w #$0000
	STX $F9
	LDY.w #$0008
	LDX.w #$0101
	LDA.b #$00
ClearObjectScratch:
	STA.l $7E7D10,X
	DEX
	BPL ClearObjectScratch
	LDA [$E0],Y
	STA $E3
	INY
	LDA [$E0],Y
	STA $E5
	INY
	REP #$20
	LDA [$E0],Y
	STA $E7
	INY
	INY
	SEP #$20
	PHA
	LDA.b #$00
	XBA
	PLA
	LDX $E7
	INX
	STX $F7
	LDX $E3
	STX $F5

	org $018CD1
ObjectInitPrologueTail:
