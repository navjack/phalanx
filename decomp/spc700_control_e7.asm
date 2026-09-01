; One-operand sequence control E7, traced at $10F6.
; Its immediate effect is clearing the direct-page work word at $52/$53.

SpcControlE7ClearWorkWord:
	MOV A,#$00              ; E8 00
	MOVW $52,YA             ; DA 52
	RET                     ; 6F
