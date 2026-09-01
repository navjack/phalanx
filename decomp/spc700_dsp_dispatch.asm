; Trace-proven periodic DSP dispatch loop at SPC ARAM $0840.
; The live Sound Test trace exercises both sides of the initial carry branch.

SpcDspDispatchLoop:
	BCS $084A               ; B0 08
	CMP $4C,$4D             ; 69 4D 4C
	BNE $0858               ; D0 11
	BBS7 $4C,$0858          ; E3 4C 0E
	MOV A,$1562+Y           ; F6 62 15: DSP register selector.
	MOV $00F2,A             ; C5 F2 00
	MOV A,$156C+Y           ; F6 6C 15: source-state table value.
	MOV X,A                 ; 5D
	MOV A,(X)               ; E6
	MOV $00F3,A             ; C5 F3 00
	DBNZ Y,$083C            ; FE E2: continue through the unpromoted gate.
