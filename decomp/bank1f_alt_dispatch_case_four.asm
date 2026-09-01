; Handle alternate event type 4.

	CMP.b #$04
	BNE AltCaseFive
	INX
	LDA.l $1F851E,X
	JSL $1F88DF
	BRL AltDispatchDone

	org $1F84C6
AltCaseFive:
