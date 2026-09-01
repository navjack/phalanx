; DSP write helper at SPC ARAM $09DB.
; Y selects the DSP register and A supplies its value.

SpcWriteDspYA:
	MOV $00F2,Y             ; CC F2 00
	MOV $00F3,A             ; C5 F3 00
	RET                     ; 6F
