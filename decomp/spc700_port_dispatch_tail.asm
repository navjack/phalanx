; Command-side continuation at SPC ARAM $15BF.
; This updates the command pointer and provides nearby DSP-port helpers.

SpcPortDispatchTail:
	MOV ($14)+Y,A           ; D7 14
	INC Y                   ; FC
	BNE $15B4               ; D0 F0
	INC $15                 ; AB 15
	BRA $15B4               ; 2F EC
	BPL $15B4               ; 10 EA
	CMP Y,$00F4             ; 5E F4 00
	BPL $15B4               ; 10 E5
	MOV A,$00F6             ; E5 F6 00
	MOV Y,$00F7             ; EC F7 00
	MOVW $14,YA             ; DA 14
	MOV Y,$00F4             ; EC F4 00
	MOV A,$00F5             ; E5 F5 00
	MOV $00F4,Y             ; CC F4 00
	BNE $15AF               ; D0 CD
	MOV X,#$31              ; CD 31
	MOV $00F1,X             ; C9 F1 00
	RET                     ; 6F

SpcWriteDspAddressY:
	MOV $00F2,Y             ; CC F2 00
	MOV A,$00F3             ; E5 F3 00
	RET                     ; 6F
