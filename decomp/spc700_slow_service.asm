; Slow periodic service path at SPC ARAM $0880.
; Its trace-proven call order runs global work and three voice-service passes.

SpcSlowService:
	MOV A,$53               ; E4 53
	POP Y                   ; EE
	MUL YA                  ; CF
	CLRC                    ; 60
	ADC A,$51               ; 84 51
	MOV $51,A               ; C4 51
	BCC $08A5               ; 90 1A
	CALL $0A4A              ; 3F 4A 0A
	MOV X,#$00              ; CD 00
	CALL $08D8              ; 3F D8 08
	CALL $0BFC              ; 3F FC 0B
	MOV X,#$01              ; CD 01
	CALL $08D8              ; 3F D8 08
	CALL $0D26              ; 3F 26 0D
	MOV X,#$03              ; CD 03
	CALL $08D8              ; 3F D8 08
	BRA $0836               ; 2F 91
