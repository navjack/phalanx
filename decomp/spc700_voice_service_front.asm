; Front half of the shared per-voice service at SPC ARAM $148B.
; Entered by both fast voice scans; branches delegate modulation/mix work.

SpcVoiceService:
	CLR7 $13                ; F2 13
	MOV A,$C1+X             ; F4 C1
	BEQ $149A               ; F0 09
	MOV A,$02E0+X           ; F5 E0 02
	CBNE $C0+X,$149A        ; DE C0 03
	CALL $1504              ; 3F 04 15
	MOV A,$0331+X           ; F5 31 03
	MOV Y,A                 ; FD
	MOV A,$0330+X           ; F5 30 03
	MOVW $10,YA             ; DA 10
	MOV A,$91+X             ; F4 91
	BEQ $14B1               ; F0 0A
	MOV A,$0341+X           ; F5 41 03
	MOV Y,A                 ; FD
	MOV A,$0340+X           ; F5 40 03
	CALL $14E6              ; 3F E6 14
	BBC7 $13,$14B7          ; F3 13 03
	CALL $1350              ; 3F 50 13
	CLR7 $13                ; F2 13
	CALL $1277              ; 3F 77 12
	MOV A,$A0+X             ; F4 A0
	BEQ $14CE               ; F0 0E
	MOV A,$A1+X             ; F4 A1
	BNE $14CE               ; D0 0A
	MOV A,$0371+X           ; F5 71 03
	MOV Y,A                 ; FD
	MOV A,$0370+X           ; F5 70 03
	CALL $14E6              ; 3F E6 14
	MOV A,$B1+X             ; F4 B1
	BEQ $1487               ; F0 B5
