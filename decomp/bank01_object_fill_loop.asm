; Copy each object record from the indirect source pointer into WRAM.

ObjectInitFill:
	LDX.w #$0001
ObjectFillLoop:
	LDA [$E0],Y
	STA.l $7E7C00,X
	INX
	INY
	DEC $F5
	BNE ObjectFillLoop
