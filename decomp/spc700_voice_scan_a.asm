; Trace-proven first fast per-voice scan at SPC ARAM $08A5.
; It visits active voice-state slots and calls the common voice service.

SpcVoiceScanA:
	MOV A,$04               ; E4 04
	BEQ $08BF               ; F0 16
	MOV X,#$00              ; CD 00
	MOV $47,#$01            ; 8F 01 47
SpcVoiceScanANext:
	MOV A,$31+X             ; F4 31
	BEQ $08B5               ; F0 03
	CALL $148B              ; 3F 8B 14
	INC X                   ; 3D
	INC X                   ; 3D
	ASL $47                 ; 0B 47
	MOV A,$47               ; E4 47
	CMP A,#$40              ; 68 40
	BNE SpcVoiceScanANext   ; D0 EF
