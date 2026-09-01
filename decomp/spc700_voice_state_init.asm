; Per-voice state initialization at SPC ARAM $0A0F.
; All Sound Test previews execute this bounded routine. It iterates the voice
; slots from X=$0A down in pairs, writes their initial state, invokes the pan
; setup path, then seeds shared control bytes.

SpcInitVoiceState:
	MOV X,#$0A              ; CD 0A
	MOV $47,#$20            ; 8F 20 47
	MOV A,#$FF              ; E8 FF
	MOV $0301+X,A           ; D5 01 03
	MOV A,#$0A              ; E8 0A
	CALL $1091              ; 3F 91 10
	MOV $0211+X,A           ; D5 11 02
	MOV $0381+X,A           ; D5 81 03
	MOV $02F0+X,A           ; D5 F0 02
	MOV $0280+X,A           ; D5 80 02
	MOV $0400+X,A           ; D5 00 04
	MOV $B1+X,A             ; D4 B1
	MOV $C1+X,A             ; D4 C1
	DEC X                   ; 1D
	DEC X                   ; 1D
	LSR $47                 ; 4B 47
	BNE $0A14               ; D0 DD
	MOV $5A,A               ; C4 5A
	MOV $68,A               ; C4 68
	MOV $54,A               ; C4 54
	MOV $50,A               ; C4 50
	MOV $42,A               ; C4 42
	MOV $5F,A               ; C4 5F
	MOV $59,#$C0            ; 8F C0 59
	MOV $53,#$20            ; 8F 20 53
	RET                     ; 6F
