; Handle alternate event type 5 with two notifications.

	CMP.b #$05
	BNE AltCaseSix
	INX
	LDA.l $1F851E,X
	JSL $1F888B
	INX
	LDA.l $1F851E,X
	JSL $1F88DF
	BRL AltDispatchDone

	org $1F84DF
AltCaseSix:
