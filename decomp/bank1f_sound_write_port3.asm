; Send a normalized sound value to APU port 3 and retain its low byte.

	REP #$20
	AND.w #$00FF
	SEP #$20
	STA.w $2143
	STA $DD
	RTL
