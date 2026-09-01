; Update timing state and select whether a transition should be applied.

StateUpdate:
	REP #$20
	AND.w #$003F
	STA.l $7ECB02
	ASL A
	ASL A
	ASL A
	TAX
	LDA.l $1F8132,X
	STA.l $7ECB04
	LDA.l $1F8134,X
	STA.l $7ECB06
	LDA.l $1F8136,X
	STA.l $7ECB08
	LDA.l $1F8138,X
	STA.l $7ECB0A
	LDA.l $7ECB00
	STA.l $7ECB0C
	LDA.l $7ECB12
	SEC
	SBC.l $7ECB0C
	STA.l $7ECB0E
	LDA.l $7ECB0C
	STA.l $7ECB12
	LDA.l $7ECB18
	BEQ StateUpdateDone
	SEC
	SBC.l $7ECB0E
	BMI StateUpdateReset
	STA.l $7ECB18
	BRA StateUpdateDone
StateUpdateReset:
	LDA.w #$0000
	STA.l $7ECB18
StateUpdateDone:
	LDA.l $7ECB18
	BNE StateUpdateActive
	JSR StateUpdateHelper
	JMP StateUpdateReturn
StateUpdateActive:
	LDA.l $7ECB04
	CMP.l $7ECB14
	BPL StateUpdateApply
	JMP StateUpdateReturn
StateUpdateApply:
	JSR StateUpdateHelper
	JMP StateUpdateReturn

	org $1F80D0
StateUpdateReturn:
