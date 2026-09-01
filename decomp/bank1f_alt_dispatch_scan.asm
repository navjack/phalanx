; Scan the alternate event table and notify both tracked state channels.

	LDA.l $1F851E,X
	CMP.b #$FF
	BNE AltScanFirst
	BRA AltScanNext
AltScanFirst:
	CMP $D4
	BNE AltScanNotify
	BRA AltScanNext
AltScanNotify:
	PHX
	STA $D4
	JSL $1F8909
	PLX
AltScanNext:
	INX
	LDA.l $1F851E,X
	CMP.b #$FF
	BNE AltScanSecond
	BRA AltScanDone
AltScanSecond:
	CMP $D6
	BNE AltScanNotifySecond
	BRA AltScanDone
AltScanNotifySecond:
	PHX
	STA $D6
	JSL $1F896F
	PLX
AltScanDone:
	INX
