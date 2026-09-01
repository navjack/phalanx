; Stream-control dispatcher at SPC ARAM $0B19.
; Non-control bytes take the note path; E0+ bytes use the resolver.

SpcDispatchSequenceControl:
	CMP A,#$E0              ; 68 E0
	BCC $0B22               ; 90 05
	CALL $101C              ; 3F 1C 10
	BRA $0ADB               ; 2F B9
