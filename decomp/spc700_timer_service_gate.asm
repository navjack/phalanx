; Timer-to-slow-service gate at SPC ARAM $0879.
; On the carry path out of the timer phase update, it reconciles `$4C/$4D`
; before falling through to the periodic service routine at `$0880`.

SpcTimerServiceGate:
	CMP $4C,$4D             ; 69 4D 4C
	BEQ $0880               ; F0 02
	INC $4C                 ; AB 4C
