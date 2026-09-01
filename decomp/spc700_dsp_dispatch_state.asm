; DSP dispatch-loop result handoff at SPC ARAM $085A.
; This is the traced bridge from the `$0840` register-table loop to the timer
; poll. It copies the current table index to two work bytes, then derives the
; two-bit state value consumed by the following unpromoted bit-operation
; sequence.

SpcDspDispatchState:
	MOV $45,Y               ; CB 45
	MOV $46,Y               ; CB 46
	MOV A,$18               ; E4 18
	EOR A,$19               ; 44 19
	LSR A                   ; 5C
	LSR A                   ; 5C
