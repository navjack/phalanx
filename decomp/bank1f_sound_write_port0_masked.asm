; Send a masked sound command through APU port 0. Sound Test capture 01
; proves this direct STA is the CPU-side origin of the observed $2140=$01.
; The nonzero path folds the command into the latched state at $D0; zero
; bypasses that transformation and sends the saved accumulator directly.

	REP #$20
	AND.w #$00FF
	SEP #$20
	BEQ SoundPort0Direct
	AND.b #$3F
	ORA $D0
	PHA
	LDA $D0
	EOR.b #$40
	AND.b #$40
	STA $D0
	PLA

	org $1F88A2
SoundPort0Direct:
	STA.w $2140
	STA $DA
	RTL
