; Per-voice helper at SPC ARAM $1504, called from the common voice service.

SpcVoiceServiceHelper:
	SET7 $13                ; E2 13
	MOV Y,$51               ; EB 51
	MOV A,$02D1+X           ; F5 D1 02
	MUL YA                  ; CF
	MOV A,Y                 ; DD
	CLRC                    ; 60
	ADC A,$02D0+X           ; 95 D0 02
	ASL A                   ; 1C
	BCC $1516               ; 90 02
	EOR A,#$FF              ; 48 FF
	MOV Y,$C1+X             ; FB C1
	MUL YA                  ; CF
	MOV A,Y                 ; DD
	EOR A,#$FF              ; 48 FF
	MOV Y,$59               ; EB 59
	MUL YA                  ; CF
	MOV A,$0210+X           ; F5 10 02
	MUL YA                  ; CF
	MOV A,$0301+X           ; F5 01 03
	MUL YA                  ; CF
	MOV A,Y                 ; DD
	MUL YA                  ; CF
	MOV A,Y                 ; DD
	MOV $0321+X,A           ; D5 21 03
	RET                     ; 6F
