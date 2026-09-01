; Select the existing work record or fall through to the new-handler path.
	org $018DD1
ObjectWorkExistingPath:

	LDA $EB
	STA $ED
	TAX
	LDA.l $7E7D10,X
	BNE ObjectWorkExisting
	BRL ObjectWorkHandlerContinue
ObjectWorkExisting:
