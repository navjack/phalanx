; Handle event type 7 with its three parameterized sound calls.

	CMP.b #$07
	BNE DispatchDone
	INX
	LDA.l $1F851E,X
	JSL $1F887E
	INX
	LDA.l $1F851E,X
	JSL $1F88A8
	INX
	LDA.l $1F851E,X
	JSL $1F88D2
	BRL DispatchDone
