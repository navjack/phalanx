; Tail of the common per-voice service at SPC ARAM $14D2.
; It either returns through the vibrato delay path or prepares the next state.

SpcVoiceServiceTail:
	MOV A,$02B0+X           ; F5 B0 02
	CBNE $B0+X,$1487        ; DE B0 AF
	MOV Y,$51               ; EB 51
	MOV A,$02A1+X           ; F5 A1 02
	MUL YA                  ; CF
	MOV A,Y                 ; DD
	CLRC                    ; 60
	ADC A,$02A0+X           ; 95 A0 02
	JMP $1467               ; 5F 67 14
	SET7 $13                ; E2 13
	MOV $12,Y               ; CB 12
	CALL $1294              ; 3F 94 12
	PUSH Y                  ; 6D
	MOV Y,$51               ; EB 51
	MUL YA                  ; CF
	MOV $14,Y               ; CB 14
	MOV $15,#$00            ; 8F 00 15
	MOV Y,$51               ; EB 51
	POP A                   ; AE
	MUL YA                  ; CF
	ADDW YA,$14             ; 7A 14
	CALL $1294              ; 3F 94 12
	ADDW YA,$10             ; 7A 10
	MOVW $10,YA             ; DA 10
	RET                     ; 6F
