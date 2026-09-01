; Normalize the accumulator width before returning from the state update.

	REP #$20
	AND.w #$00FF
	SEP #$20
	RTL
