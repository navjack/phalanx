; Install the existing work record as the active object.

ObjectWorkHandler:
	LDX $E9
	LDA.l $7E8140,X
	STA $ED
	TXA
	PLX
	INX
	STA.l $7E4000,X
	INX
	LDA $ED
	STA.l $7E4000,X
	PHX
	LDX $E9
	LDA.l $7E7F30,X
	STA $EB
	BRA ObjectWorkExistingPath
