; E0 descriptor-to-DSP setup loop at SPC ARAM $105D.
; It reads descriptor bytes, selects DSP registers through X, and stores the
; descriptor's final pair into per-voice state.

SpcSequenceE0DspSetup:
	MOV Y,#$00              ; 8D 00
	MOV A,($14)+Y           ; F7 14
	BPL $1071               ; 10 0E
	AND A,#$1F              ; 28 1F
	AND $48,#$20            ; 38 20 48
	TSET $0048,A            ; 0E 48 00
	OR $49,$47              ; 09 47 49
	MOV A,Y                 ; DD
	BRA $1078               ; 2F 07
	MOV A,$47               ; E4 47
	TCLR $0049,A            ; 4E 49 00
	MOV A,($14)+Y           ; F7 14
	MOV $00F2,X             ; C9 F2 00
	MOV $00F3,A             ; C5 F3 00
	INC X                   ; 3D
	INC Y                   ; FC
	CMP Y,#$04              ; AD 04
	BNE $1076               ; D0 F2
	POP X                   ; CE
	MOV A,($14)+Y           ; F7 14
	MOV $0221+X,A           ; D5 21 02
	INC Y                   ; FC
	MOV A,($14)+Y           ; F7 14
	MOV $0220+X,A           ; D5 20 02
	RET                     ; 6F
