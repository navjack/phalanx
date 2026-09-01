; Clear the new object's four-byte work record before entering its handler.

	INX
	LDA.b #$00
	STA.l $7E4000,X
	INX
	STA.l $7E4000,X
	INX
	STA.l $7E4000,X
	INX
	STA.l $7E4000,X
	PHX
	BRA ObjectWorkHandler
