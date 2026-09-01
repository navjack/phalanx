; Maintain forced-blank state around a short PPU update. The low nibble of
; $023D is a countdown; once it expires, the display is held at $80.

MaintainForcedBlank:
	LDA.w $023D
	BMI ForcedBlankDone
	LDA.w $0275
	BPL ForcedBlankVisible
ForcedBlankCountdown:
	LDA.b #$03
	JSL $008342
	LDA.w $023D
	AND.b #$0F
	DEC A
	STA.w $023D
	STA.w $2100
	BNE ForcedBlankCountdown
ForcedBlankVisible:
	LDA.b #$80
	STA.w $023D
	STA.w $2100
ForcedBlankDone:
	RTL
