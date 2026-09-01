; Handle alternate event type 2.

	CMP.b #$02
	BNE AltCaseThree
	INX
	LDA.l $1F851E,X
	JSL $1F88B5
	BRL AltDispatchDone

	org $1F849D
AltCaseThree:
