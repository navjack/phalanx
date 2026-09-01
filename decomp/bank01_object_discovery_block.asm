; Resolve one object index and either record it or allocate a fresh work slot.

	LDA [$E0],Y
	INY
	STA $EB
	STA $E9
	LDX $E9
	LDA.l $7E7D10,X
	STA $F3
	BNE ObjectDiscoveryAllocate
	TXA
	LDX $F9
	STA.l $7E87FC,X
	INX
	STX $F9
	BRA ObjectBranchLoop
ObjectDiscoveryAllocate:
	LDX $E9
	LDA.l $7E7D10,X
	STA $E9
	PLX
