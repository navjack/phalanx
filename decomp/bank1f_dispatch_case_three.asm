; Handle event type 3 with its two parameterized sound calls.

	CMP.b #$03
	BNE DispatchCaseFour
	INX
	LDA.l $1F851E,X
	JSL $1F887E
	INX
	LDA.l $1F851E,X
	JSL $1F88A8
	BRL DispatchDone

	org $1F83C0
DispatchCaseFour:
