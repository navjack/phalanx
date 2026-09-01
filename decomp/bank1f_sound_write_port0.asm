; Send a 16-bit sound value to APU port 0 and retain its low byte.
; Callers: bank-$1F event dispatch cases.
; Entry: accumulator width is caller-defined; the routine normalizes it.

	REP #$20
	AND.w #$00FF
	SEP #$20
	STA.w $2140
	STA $DA
	RTL
