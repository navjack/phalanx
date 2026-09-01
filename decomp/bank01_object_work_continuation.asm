; Continue the work-record chain and return to the appropriate scan path.

ObjectSearchNew:
	LDA $ED
	STA $EB

	; Entry used by the new-work handler branch at $8E30.
ObjectWorkHandlerContinue:
	LDA $EB
	LDX $F9
	STA.l $7E87FC,X
	INX
	STX $F9
	PLX
	LDA.l $7E4000,X
	STA $ED
	DEX
	LDA.l $7E4000,X
	STA $E9
	BEQ ObjectContinuationDone
	DEX
	LDA $ED
	STA $EB
	PHX
	BRL ObjectWorkExistingPath
ObjectContinuationDone:
	DEX
	PHX
	BRL ObjectBranchLoop
