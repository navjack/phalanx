; Reset entry: $00:8000, emulation-mode M=1 X=1.
; The initialization deliberately uses the hardware's width transitions and
; the native SNES memory map; keep the explicit immediate widths intact.

Reset:
	CLC
	XCE                         ; Enter native mode; CLC makes E=0.
	REP #$10                   ; X/Y become 16-bit.
	LDX.w #$01FF
	TXS                         ; Put the native stack at $01FF.
	SEP #$20                   ; A becomes 8-bit.
	PHA
	LDA.b #$00
	XBA
	PLA
	LDA.b #$00
	PHA
	PLB                         ; DB = $00.
	REP #$20                   ; A becomes 16-bit.
	LDA.w #$0000
	TCD                         ; Direct page = $0000.
	SEP #$20                   ; A becomes 8-bit again.
	PHA
	LDA.b #$00
	XBA
	PLA
	LDA.b #$80
	STA $2100                   ; Force blank while VRAM/CGRAM are cleared.
	STZ $4200                   ; Disable NMI and auto-joypad polling.

	REP #$20
	LDX.w #$25FE
	LDA.w #$0000

ClearWRAM:
	STA $7E0000,X
	DEX
	DEX
	BPL ClearWRAM

	LDX.w #$3000
	LDA.w #$0000

ClearVRAM:
	STA $7E0000,X
	INX
	INX
	BNE ClearVRAM

ClearVRAM7F:
	STA $7F0000,X
	INX
	INX
	BNE ClearVRAM7F

	LDA.w #$0000
	STA $7E2C00
	AND.w #$00FF
	SEP #$20
	LDA.b #$80
	STA $023D
	STZ $0275
	LDA.b #$01
	STA $00
	PHB
	LDA.b #$01
	PHA
	PLB                         ; DB = $01 for the bank-$01 setup call.
	JSL $018000
	PLB                         ; Restore DB = $00.
	LDA.b #$FF
	STA $0276
	STA $4201
	STZ $4202
	STZ $4203
	STZ $4204
	STZ $4205
	STZ $4206
	STZ $4207
	STZ $4208
	STZ $4209
	STZ $420A
	STZ $0264
	STZ $420B
	STZ $0265
	STZ $420C
	JSL $0080ED
	LDX.w #$0400
	STX $4300
	LDX.w #$1801
	STX $4310
	LDX.w #$1604
	STX $4320
	LDX.w #$2200
	STX $4330
	JSL $0080E1
	JML $008845
