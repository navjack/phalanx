; Per-voice note-tick gate at SPC ARAM $1414.
; The sequence scanner and delay-flow paths both arrive here.  Live preview
; traces execute the common path through $1439 on every active voice tick;
; the two earlier branches cover per-voice countdown/state updates.

SpcNoteTickGate:
	MOV A,$47               ; E4 47
	MOV Y,#$5C              ; 8D 5C
	CALL $09D3              ; 3F D3 09: restore this voice's DSP selector state.
	CLR7 $13                ; F2 13
	MOV A,$A0+X             ; F4 A0
	BEQ $1434               ; F0 13
	MOV A,$A1+X             ; F4 A1
	BEQ $1429               ; F0 04
	DEC $A1+X               ; 9B A1
	BRA $1434               ; 2F 0B
	SET7 $13                ; E2 13
	MOV A,#$60              ; E8 60
	MOV Y,#$03              ; 8D 03
	DEC $A0+X               ; 9B A0
	CALL $1391              ; 3F 91 13
	CALL $1277              ; 3F 77 12
	MOV A,$B1+X             ; F4 B1
	BEQ $1487               ; F0 4C: inactive voice bypasses vibrato processing.
