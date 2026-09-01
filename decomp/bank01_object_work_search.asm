; Search the work table for an existing object relationship.

ObjectWorkSearch:
	TAX
	CPX $E9
	BMI ObjectSearchFound
	LDA $E9
	STA $EB
	LDX $ED
	LDA.l $7E7D10,X
	STA $E9
	ObjectWorkSearchLoop:
	LDX $E9
	LDA.l $7E7E20,X
	STA $E9
	BEQ ObjectSearchNew
	TAX
	CPX $EB
	BMI ObjectWorkHandler
	BEQ ObjectWorkHandler
	BRA ObjectWorkSearchLoop
ObjectSearchFound:
	LDX $ED
	LDA.l $7E7D10,X
	STA $E9
