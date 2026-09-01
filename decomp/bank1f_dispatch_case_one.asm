; Handle event type 1 and return to the common dispatcher epilogue.

	CMP.b #$01
	BNE DispatchCaseTwo
	INX
	LDA.l $1F851E,X
	JSL $1F887E
	BRL DispatchDone

	org $1F8397
DispatchCaseTwo:
