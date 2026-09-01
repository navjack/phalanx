; Enter the event dispatcher and compute the selector-table byte offset.

DispatchEvents:
	PHX
	PHY
	PHP
	REP #$20
	AND.w #$00FF
	STA $D8
	ASL A
	ASL A
	CLC
	ADC $D8
	CLC
	ADC $D8
	TAX
	REP #$20
	AND.w #$00FF
	SEP #$20
