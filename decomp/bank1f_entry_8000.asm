; Initialize the bank-$1F runtime state and issue the first sound-engine calls.

Bank1FEntry:
	REP #$20
	AND.w #$00FF
	SEP #$20
	STZ $D0
	STZ $D1
	STZ $D2
	STZ $D3
	STZ $D4
	STZ $D5
	JSL $1F8115
	LDX.w #$B188
	STX $10
	LDA.b #$1E
	STA $12
	JSL $1F8A3B
	LDA.b #$02
	JSL $1F8909
	LDA.b #$01
	JSL $1F8909
	LDA.b #$01
	JSL $1F896F
	REP #$20
	AND.w #$00FF
	SEP #$20
	RTL
