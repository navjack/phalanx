; Handle event type 4.

	CMP.b #$04
	BNE DispatchCaseFive
	INX
	LDA.l $1F851E,X
	JSL $1F88D2
	BRL DispatchDone

	org $1F83D0
DispatchCaseFive:
