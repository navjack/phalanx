; Build the object index/work table from the freshly copied records.

ObjectIndexSetup:
	LDX $E3
	DEX
	STX $F5
	LDX.w #$0001
	STX $E9
	STZ $EB
	STZ $EC
	STZ $ED
	STZ $EE
ObjectIndexLoop:
	LDX $E9
	LDA.l $7E7C00,X
	STA $EB
	TAX
	LDA.l $7E7D10,X
	STA $F7
	LDX $E9
	STA.l $7E7E20,X
	TXA
	LDX $EB
	STA.l $7E7D10,X
	LDX $E9
	INX
	STX $E9
	LDX $F5
	DEX
	STX $F5
	BPL ObjectIndexLoop
