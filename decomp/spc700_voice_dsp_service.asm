; Shared per-voice DSP service at SPC ARAM $0950.
; The vibrato routine jumps here and live Sound Test traces execute this whole
; bounded path before the `$09D3` DSP-selector gate. It derives a voice-local
; pair from `$10/$11`, descriptor tables, and `$0220/$0221+X`, then selects
; the pending DSP register/value held in Y/A.

SpcVoiceDspService:
	MOV Y,#$00              ; 8D 00
	MOV A,$11               ; E4 11
	SETC                    ; 80
	SBC A,#$34              ; A8 34
	BCS $0962               ; B0 09
	MOV A,$11               ; E4 11
	SETC                    ; 80
	SBC A,#$13              ; A8 13
	BCS $0966               ; B0 06
	DEC Y                   ; DC
	ASL A                   ; 1C
	ADDW YA,$10             ; 7A 10
	MOVW $10,YA             ; DA 10
	PUSH X                  ; 4D
	MOV A,$11               ; E4 11
	ASL A                   ; 1C
	MOV Y,#$00              ; 8D 00
	MOV X,#$18              ; CD 18
	DIV YA,X                ; 9E
	MOV X,A                 ; 5D
	MOV A,$1578+Y           ; F6 78 15
	MOV $15,A               ; C4 15
	MOV A,$1577+Y           ; F6 77 15
	MOV $14,A               ; C4 14
	MOV A,$157A+Y           ; F6 7A 15
	PUSH A                  ; 2D
	MOV A,$1579+Y           ; F6 79 15
	POP Y                   ; EE
	SUBW YA,$14             ; 9A 14
	MOV Y,$10               ; EB 10
	MUL YA                  ; CF
	MOV A,Y                 ; DD
	MOV Y,#$00              ; 8D 00
	ADDW YA,$14             ; 7A 14
	MOV $15,Y               ; CB 15
	ASL A                   ; 1C
	ROL $15                 ; 2B 15
	MOV $14,A               ; C4 14
	BRA $0999               ; 2F 04
	LSR $15                 ; 4B 15
	ROR A                   ; 7C
	INC X                   ; 3D
	CMP X,#$06              ; C8 06
	BNE $0995               ; D0 F8
	MOV $14,A               ; C4 14
	POP X                   ; CE
	MOV A,$0220+X           ; F5 20 02
	MOV Y,$15               ; EB 15
	MUL YA                  ; CF
	MOVW $16,YA             ; DA 16
	MOV A,$0220+X           ; F5 20 02
	MOV Y,$14               ; EB 14
	MUL YA                  ; CF
	PUSH Y                  ; 6D
	MOV A,$0221+X           ; F5 21 02
	MOV Y,$14               ; EB 14
	MUL YA                  ; CF
	ADDW YA,$16             ; 7A 16
	MOVW $16,YA             ; DA 16
	MOV A,$0221+X           ; F5 21 02
	MOV Y,$15               ; EB 15
	MUL YA                  ; CF
	MOV Y,A                 ; FD
	POP A                   ; AE
	ADDW YA,$16             ; 7A 16
	MOVW $16,YA             ; DA 16
	MOV A,X                 ; 7D
	XCN A                   ; 9F
	LSR A                   ; 5C
	OR A,#$02               ; 08 02
	MOV Y,A                 ; FD
	MOV A,$16               ; E4 16
	CALL $09D3              ; 3F D3 09
	INC Y                   ; FC
	MOV A,$17               ; E4 17
