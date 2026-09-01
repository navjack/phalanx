; Trace-proven second fast per-voice scan at SPC ARAM $08BF.
; This scans the high voice-state slots using the same common service routine.

SpcVoiceScanB:
	MOV A,$05               ; E4 05
	BEQ $08D5               ; F0 12
	MOV X,#$0C              ; CD 0C
	MOV $47,#$40            ; 8F 40 47
SpcVoiceScanBNext:
	MOV A,$31+X             ; F4 31
	BEQ $08CF               ; F0 03
	CALL $148B              ; 3F 8B 14
	INC X                   ; 3D
	INC X                   ; 3D
	ASL $47                 ; 0B 47
	BNE SpcVoiceScanBNext   ; D0 F3
