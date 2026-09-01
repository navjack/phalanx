; Handle alternate event type 7 with three notifications.

	CMP.b #$07
	BNE AltDispatchUnknown
	INX
	LDA.l $1F851E,X
	JSL $1F888B
	INX
	LDA.l $1F851E,X
	JSL $1F88B5
	INX
	LDA.l $1F851E,X
	JSL $1F88DF
	BRL AltDispatchDone

	org $1F851A
AltDispatchUnknown:
