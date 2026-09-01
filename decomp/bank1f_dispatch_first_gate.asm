; Select the first event-type handler or return to the dispatcher epilogue.

	LDA.l $1F851E,X
	BNE DispatchCaseOne
	BRL DispatchDone

	org $1F8387
DispatchCaseOne:
	org $1F8424
DispatchDone:
