; Handle alternate event type 1.

	CMP.b #$01
	BNE AltCaseTwo
	INX
	LDA.l $1F851E,X
	JSL $1F888B
	BRL AltDispatchDone

	org $1F848D
AltCaseTwo:
