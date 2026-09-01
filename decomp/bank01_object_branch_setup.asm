; Prepare the next object-record pass and select the appropriate continuation.

	LDX $E7
	INX
	STX $F7
	LDX.w #$0001
	STX $F5
	STZ $E9
	STZ $EA
	LDX $F7
	DEX
	STX $F7
	BNE ObjectBranchContinue
	LDA $E5
	BNE ObjectBranchShort
	BRL ObjectInitReturn
	BRL ObjectInitPrologueTail

; Branch targets are anchored here for the later graph regions.
	org $018D81
ObjectBranchLoop:
	org $018D88
ObjectBranchExitShort:
	org $018D92
ObjectBranchContinue:
	org $018D8F
ObjectBranchShort:
	org $018E74
ObjectBranchExit:
