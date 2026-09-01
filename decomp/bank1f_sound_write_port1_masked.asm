; Send a masked sound command through APU port 1. Sound Test capture 01
; proves this direct STA is the CPU-side origin of the observed $2141=$00.

	REP #$20
	AND.w #$00FF
	SEP #$20
	BEQ SoundPort1Direct
	AND.b #$3F
	ORA $D1
	PHA
	LDA $D1
	EOR.b #$40
	AND.b #$40
	STA $D1
	PLA

	org $1F88CC
SoundPort1Direct:
	STA.w $2141
	STA $DB
	RTL
