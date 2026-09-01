; Scan the next event-table entries and issue state-change notifications.

DispatchScan:
	LDA.l $1F851E,X
	CMP.b #$FF
	BNE DispatchScanFirst
	BRA DispatchScanNext
DispatchScanFirst:
	CMP $D4
	BNE DispatchScanNotify
	BRA DispatchScanNext
DispatchScanNotify:
	PHX
	STA $D4
	JSL $1F8909
	PLX
DispatchScanNext:
	INX
	LDA.l $1F851E,X
	CMP.b #$FF
	BNE DispatchScanSecond
	BRA DispatchScanDone
DispatchScanSecond:
	CMP $D6
	BNE DispatchScanNotifySecond
	BRA DispatchScanDone
DispatchScanNotifySecond:
	PHX
	STA $D6
	JSL $1F896F
	PLX
DispatchScanDone:
	INX
