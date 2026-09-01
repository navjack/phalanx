; CPU-to-SPC handshake entry at SPC ARAM $15A1.
; Live Sound Test port traces reach the wait/read/acknowledge path below.

SpcPortHandshake:
	MOV A,#$BB              ; E8 BB
	MOV $00F5,A             ; C5 F5 00: acknowledge marker.
SpcWaitCpuMarker:
	MOV A,$00F4             ; E5 F4 00
	CMP A,#$CC              ; 68 CC
	BNE SpcWaitCpuMarker    ; D0 F9
	BRA $15CF               ; 2F 20
SpcWaitPortChange:
	MOV Y,$00F4             ; EC F4 00
	BNE SpcWaitPortChange   ; D0 FB
	CMP Y,$00F4             ; 5E F4 00
	BNE $15C8               ; D0 0F
	MOV A,$00F5             ; E5 F5 00
	MOV $00F4,Y             ; CC F4 00
