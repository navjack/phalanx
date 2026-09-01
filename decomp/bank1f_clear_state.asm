; Clear the cached transition tuple before dispatch begins.

ClearBank1FState:
	REP #$20
	LDA.w #$0000
	STA.l $7ECB14
	STA.l $7ECB16
	STA.l $7ECB18
	STA.l $7ECB1A
	REP #$20
	AND.w #$00FF
	SEP #$20
	RTL
