; Sequence control EF, traced at $1167.
; It stores a target pair/count in per-voice state and rebases stream work.

SpcSequenceEF:
	MOV $0240+X,A           ; D5 40 02
	CALL $102E              ; 3F 2E 10
	MOV $0241+X,A           ; D5 41 02
	CALL $102E              ; 3F 2E 10
	MOV $80+X,A             ; D4 80
	MOV A,$30+X             ; F4 30
	MOV $0230+X,A           ; D5 30 02
	MOV A,$31+X             ; F4 31
	MOV $0231+X,A           ; D5 31 02
	MOV A,$0240+X           ; F5 40 02
	MOV $30+X,A             ; D4 30
	MOV A,$0241+X           ; F5 41 02
	MOV $31+X,A             ; D4 31
	RET                     ; 6F
