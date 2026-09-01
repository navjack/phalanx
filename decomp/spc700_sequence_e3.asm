; Sequence control E3: vibrato delay/depth/rate, traced at $10B8.
; The values are consumed by the proven $143B voice-vibrato service.

SpcSequenceE3Vibrato:
	MOV $02B0+X,A           ; D5 B0 02
	CALL $102E              ; 3F 2E 10
	MOV $02A1+X,A           ; D5 A1 02
	CALL $102E              ; 3F 2E 10
	MOV $B1+X,A             ; D4 B1
	MOV $02C1+X,A           ; D5 C1 02
	MOV A,#$00              ; E8 00
	MOV $02B1+X,A           ; D5 B1 02
	RET                     ; 6F
