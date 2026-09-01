; Sequence control ED: per-voice volume level, traced at $1141.

SpcSequenceEDVolume:
	MOV $0301+X,A           ; D5 01 03
	MOV A,#$00              ; E8 00
	MOV $0300+X,A           ; D5 00 03
	RET                     ; 6F
