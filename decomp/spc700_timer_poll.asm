; Trace-proven timer-0 poll and periodic-service entry at SPC ARAM $0869.
; Timer reads reset the counter; a nonzero tick advances the service phase.

SpcTimerPoll:
	MOV Y,$00FD             ; EC FD 00: timer-0 counter read/reset.
	BEQ SpcTimerPoll        ; F0 FB
	PUSH Y                  ; 6D
	MOV A,#$20              ; E8 20
	MUL YA                  ; CF
	CLRC                    ; 60
	ADC A,$43               ; 84 43
	MOV $43,A               ; C4 43
	BCC $0880               ; 90 07: periodic service when phase carries.
