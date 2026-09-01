; Handle event type 5 with two parameterized sound calls.

	CMP.b #$05
	BNE DispatchCaseSix
	INX
	LDA.l $1F851E,X
	JSL $1F887E
	INX
	LDA.l $1F851E,X
	JSL $1F88D2
	BRL DispatchDone

	org $1F83E9
DispatchCaseSix:
