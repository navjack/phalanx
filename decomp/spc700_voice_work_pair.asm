; Shared per-voice work-pair loader at SPC ARAM $1277.
; Both the note-tick gate and the common voice service call this exact helper.
; It copies the voice-local `$0360/$0361+X` pair into the direct-page `$10/$11`
; work pair consumed by their downstream service paths.

SpcLoadVoiceWorkPair:
	MOV A,$0361+X           ; F5 61 03
	MOV $11,A               ; C4 11
	MOV A,$0360+X           ; F5 60 03
	MOV $10,A               ; C4 10
	RET                     ; 6F
