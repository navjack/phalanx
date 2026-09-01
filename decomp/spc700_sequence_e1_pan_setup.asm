; E1 pan continuation at SPC ARAM $109F.
; It derives complementary per-voice pan state and invokes the pan helper.

SpcSequenceE1PanSetup:
	MOV $91+X,A             ; D4 91
	PUSH A                  ; 2D
	CALL $102E              ; 3F 2E 10
	MOV $0350+X,A           ; D5 50 03
	SETC                    ; 80
	SBC A,$0331+X           ; B5 31 03
	POP X                   ; CE
	CALL $1282              ; 3F 82 12
	MOV $0340+X,A           ; D5 40 03
	MOV A,Y                 ; DD
	MOV $0341+X,A           ; D5 41 03
	RET                     ; 6F
