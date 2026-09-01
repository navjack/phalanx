; Sequence control E1: per-voice pan, traced at $1091.

SpcSequenceE1Pan:
	MOV $0351+X,A           ; D5 51 03
	AND A,#$1F              ; 28 1F
	MOV $0331+X,A           ; D5 31 03
	MOV A,#$00              ; E8 00
	MOV $0330+X,A           ; D5 30 03
	RET                     ; 6F
