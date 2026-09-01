; Commit the selected state tuple and notify the sound engine when needed.

StateUpdateHelper:
	REP #$20
	LDA.l $7ECB04
	STA.l $7ECB14
	LDA.l $7ECB06
	STA.l $7ECB16
	LDA.l $7ECB08
	STA.l $7ECB18
	LDA.l $7ECB02
	STA.l $7ECB1A
	LDA.l $7ECB0A
	AND.w #$00FF
	BEQ StateHelperDone
	REP #$20
	AND.w #$00FF
	SEP #$20
	LDA.l $7ECB0A
	JSL $1F88B5
StateHelperDone:
	REP #$20
	RTS
