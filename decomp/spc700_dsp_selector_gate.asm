; DSP-selector gate at SPC ARAM $09D3.
; The note-tick gate calls this before the `$09DB` DSP address/data writer.
; It preserves the pending accumulator value while testing the selected voice
; state in `$47/$1A`; a set condition skips directly to that writer's RET.

SpcDspSelectorGate:
	PUSH A                  ; 2D
	MOV A,$47               ; E4 47
	AND A,$1A               ; 24 1A
	POP A                   ; AE
	BNE $09E1               ; D0 06
