; Trace-proven beginning of the resident SPC700 program.
; Loaded by the 65816 from USA-ROM $1E:B242 to SPC ARAM $0800.
; The surrounding build emits this through Asar's SPC700-inline mode: the
; instructions retain their ARAM addresses while their bytes occupy the ROM
; upload payload. Runtime traces prove the initialization loop and setup calls.

SpcInit:
	CLRP                    ; 20: direct-page base = $0000.
	MOV X,#$CF              ; CD CF: stack begins at $00CF.
	MOV SP,X                ; BD
	MOV A,#$00              ; E8 00
	MOV X,A                 ; 5D
SpcInitClearLowPage:
	MOV (X+),A              ; AF: clear low ARAM through $DF.
	CMP X,#$E0              ; C8 E0
	BNE SpcInitClearLowPage ; D0 FB
	INC A                   ; BC
	CALL $11E7              ; 3F E7 11
	SET1 $48.5              ; A2 48: mark initialization complete.
	MOV A,#$60              ; E8 60
	MOV Y,#$0C              ; 8D 0C
	CALL $09DB              ; 3F DB 09
	MOV Y,#$1C              ; 8D 1C
	CALL $09DB              ; 3F DB 09
	MOV A,#$3C              ; E8 3C
	MOV Y,#$5D              ; 8D 5D
	CALL $09DB              ; 3F DB 09
	MOV A,#$F0              ; E8 F0
	MOV $00F1,A             ; C5 F1 00: enable IPL/timer state.
	MOV A,#$10              ; E8 10
	MOV $00FA,A             ; C5 FA 00: timer-0 target.
	MOV $53,A               ; C4 53: retain the initialization value in $53.
	MOV A,#$01              ; E8 01
	MOV $00F1,A             ; C5 F1 00
	MOV A,$1B               ; E4 1B
	BNE $088B               ; D0 51: enter timer/service setup.
