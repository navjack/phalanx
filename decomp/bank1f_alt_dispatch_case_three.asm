; Handle alternate event type 3 with two notifications.

	CMP.b #$03
	BNE AltCaseFour
	INX
	LDA.l $1F851E,X
	JSL $1F888B
	INX
	LDA.l $1F851E,X
	JSL $1F88B5
	BRL AltDispatchDone

	org $1F84B6
AltCaseFour:
