; DSP dispatch-loop entry gate at SPC ARAM $083A.
; The existing `$0840` body decrements Y back to `$083C`; this gate seeds its
; table index and takes the dedicated short path for index five.

SpcDspDispatchGate:
	MOV Y,#$0A              ; 8D 0A
	CMP Y,#$05              ; AD 05
	BEQ $0847               ; F0 07
