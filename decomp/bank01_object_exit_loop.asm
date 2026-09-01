; Emit the remaining object records into the discovered-index table.

ObjectInitExit:
	LDX $E7
	DEX
	STX $F7
ObjectExitLoop:
	LDA [$E0],Y
	INY
	LDX $F9
	STA.l $7E87FC,X
	INX
	STX $F9
	LDX $F7
	DEX
	STX $F7
	BPL ObjectExitLoop
	BRL ObjectBranchExitShort

ObjectInitReturn:
	PLX
	RTL
