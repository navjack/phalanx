; Final DSP-dispatch state/flag bridge at SPC ARAM $0864.
; It consumes the two-bit value just derived at `$085A` and completes the
; carry/state update before the timer-0 poll at `$0869`.

SpcDspDispatchFlags:
	NOTC                    ; ED
	ROR $18                 ; 6B 18
	ROR $19                 ; 6B 19
