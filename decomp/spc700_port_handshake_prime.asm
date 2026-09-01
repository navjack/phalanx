; CPU/SPC handshake primer at SPC ARAM $159C.
; Called by the universal stream-pointer setup path, then falls through into
; the existing `$15A1` handshake loop.

SpcPrimePortHandshake:
	MOV A,#$AA              ; E8 AA
	MOV $00F4,A             ; C5 F4 00
