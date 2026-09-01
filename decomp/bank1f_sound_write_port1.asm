; Send a normalized sound value to APU port 1 and retain its low byte.

	REP #$20
	AND.w #$00FF
	SEP #$20
	STA.w $2141
	STA $DB
	RTL
