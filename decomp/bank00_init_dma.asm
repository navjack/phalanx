; Initialize the display/DMA state after the PPU has reached a safe point.
; The calls below seed the dependent tables before the one-shot DMA transfer.

InitDisplayDMA:
	JSL $0081C4
	JSL $0081E2
	JSL $008200
	JSL $00821E
	JSL $00825C
	JSL $008279
	JSL $008296
	JSL $0082B3
	JSL $0081AD
	JSL $008186

WaitVBlankSet:
	LDA.w $4210
	BMI WaitVBlankSet
WaitVBlankClear:
	LDA.w $4210
	BPL WaitVBlankClear
	LDA.b #$00
WaitDelay:
	INC A
	BNE WaitDelay
	JSL $0080D5

	LDX.w #$0000
	STX $4E
	STX.w $2102
	LDX.w #$0400
	STX.w $4300
	LDX.w #$02E0
	STX.w $4302
	LDA.b #$00
	STA.w $4304
	LDX.w #$0220
	STX.w $4305
	LDA.b #$01
	STA.w $420B

	LDX.w #$0000
	STX $50
	STX $52
	STX $54
	STX $56
	STX $58
	STX $5A
	STX $5C
	STX $5E
	STX $5C
	STX $5E
	JSL $0080E1
	RTL
