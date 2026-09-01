; Handle event type 6 with two parameterized sound calls.

	CMP.b #$06
	BNE DispatchCaseSeven
	INX
	LDA.l $1F851E,X
	JSL $1F88A8
	INX
	LDA.l $1F851E,X
	JSL $1F88D2
	BRL DispatchDone

	org $1F8402
DispatchCaseSeven:
