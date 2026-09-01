; Copy the secondary object streams into their WRAM work areas.

ObjectCopyStreamA:
	LDA $E3
	STA $F5
	LDX.w #$0001
ObjectCopyALoop:
	LDA [$E0],Y
	STA.l $7E7F30,X
	INX
	INY
	DEC $F5
	BNE ObjectCopyALoop

	LDA $E3
	STA $F5
	LDX.w #$0001
ObjectCopyBLoop:
	LDA [$E0],Y
	STA.l $7E8140,X
	INX
	INY
	DEC $F5
	BNE ObjectCopyBLoop
