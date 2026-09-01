; Send a masked sound command through APU port 2.

	REP #$20
	AND.w #$00FF
	SEP #$20
	BEQ SoundPort2Direct
	AND.b #$3F
	ORA $D2
	PHA
	LDA $D2
	EOR.b #$40
	AND.b #$40
	STA $D2
	PLA

	org $1F88F6
SoundPort2Direct:
	STA.w $2142
	STA $DC
	RTL
