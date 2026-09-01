; Select the first alternate event handler or return to the epilogue.

	LDA.l $1F851E,X
	BNE AltCaseOne
	BRL AltDispatchDone

	org $1F847D
AltCaseOne:
	org $1F851A
AltDispatchDone:
