; Handle alternate event type 6 with two notifications.

	CMP.b #$06
	BNE AltCaseSeven
	INX
	LDA.l $1F851E,X
	JSL $1F88B5
	INX
	LDA.l $1F851E,X
	JSL $1F88DF
	BRL AltDispatchDone

	org $1F84F8
AltCaseSeven:
