; Per-voice vibrato service at SPC ARAM $143B, reached from $148B.
; Live state/DSP traces prove its E3-configured state reaches the pitch writer.

SpcServiceVibrato:
	MOV A,$02B0+X           ; F5 B0 02
	CBNE $B0+X,$1485        ; DE B0 44
	MOV A,$0100+X           ; F5 00 01
	CMP A,$02B1+X           ; 75 B1 02
	BNE $144E               ; D0 05
	MOV A,$02C1+X           ; F5 C1 02
	BRA $145B               ; 2F 0D
	SETP                    ; 40
	INC $00+X               ; BB 00
	CLRP                    ; 20
	MOV Y,A                 ; FD
	BEQ $1457               ; F0 02
	MOV A,$B1+X             ; F4 B1
	CLRC                    ; 60
	ADC A,$02C0+X           ; 95 C0 02
	MOV $B1+X,A             ; D4 B1
	MOV A,$02A0+X           ; F5 A0 02
	CLRC                    ; 60
	ADC A,$02A1+X           ; 95 A1 02
	MOV $02A0+X,A           ; D5 A0 02
	MOV $12,A               ; C4 12
	ASL A                   ; 1C
	ASL A                   ; 1C
	BCC $146F               ; 90 02
	EOR A,#$FF              ; 48 FF
	MOV Y,A                 ; FD
	MOV A,$B1+X             ; F4 B1
	CMP A,#$F1              ; 68 F1
	BCC $147B               ; 90 05
	AND A,#$0F              ; 28 0F
	MUL YA                  ; CF
	BRA $147F               ; 2F 04
	MUL YA                  ; CF
	MOV A,Y                 ; DD
	MOV Y,#$00              ; 8D 00
	CALL $14FC              ; 3F FC 14
	JMP $0950               ; 5F 50 09
	INC $B0+X               ; BB B0
	BBS7 $13,$1482          ; E3 13 F8
	RET                     ; 6F
