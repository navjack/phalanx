; NMI entry prologue: $00:8416. The handler preserves the interrupted
; M/X state in the saved PHP frame, then normalizes A before inspecting RDNMI.

	PHB
	PHP
	REP #$38
	PHX
	PHY
	PHA
	LDA.w #$0000
	SEP #$20
	PHA
	PLB                         ; DB = $00 while reading PPU state.
	CMP $4210                   ; Acknowledge/read RDNMI.
	LDA $7E2C00
	BEQ NMI_Idle                ; Skip the frame-service dispatch when idle.
	PHB
	LDA.b #$1F
	PHA
	PLB
	JSL $1FE748                ; Cross-bank frame service; exit state is not
	                            ; yet proven by the analyzer.
	PLB
	REP #$20
	JMP $883F                  ; Common epilogue after frame service.

NMI_Idle:
