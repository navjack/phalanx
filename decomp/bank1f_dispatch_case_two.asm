; Handle event type 2 and return to the common dispatcher epilogue.

	CMP.b #$02
	BNE DispatchCaseThree
	INX
	LDA.l $1F851E,X
	JSL $1F88A8
	BRL DispatchDone

	org $1F83A7
DispatchCaseThree:
