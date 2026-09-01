; Preliminary SPC700 listing from the first verified IPL upload.
;
; This file is analysis-only for now: the source is not yet assigned a ROM
; layout segment because the upload image lives in SPC ARAM, not directly in
; the 65816 ROM. Each instruction boundary below is grounded in the captured
; ARAM bytes at $0800. Live Sound Test execution traces additionally prove
; the routine blocks below; still-unclassified bytes remain deliberately open.

	org $0800
SpcInit:
	CLRP                    ; 20: direct-page base = $0000.
	MOV X,#$CF              ; CD CF
	MOV SP,X                ; BD: stack at $00CF during initialization.
	MOV A,#$00              ; E8 00
	MOV X,A                 ; 5D
	MOV (X)+,A              ; AF: clear the low ARAM page while X advances.
	CMP X,#$E0              ; C8 E0
	BNE SpcInitClearLoop    ; D0 FB
	INC A                   ; BC
	CALL $11E7              ; 3F E7 11
	SET1 $48.5              ; A2 48: mark the SPC initialization state.
	MOV A,#$60              ; E8 60
	MOV Y,#$0C              ; 8D 0C
	CALL $09DB              ; 3F DB 09
	MOV Y,#$1C              ; 8D 1C
	CALL $09DB              ; 3F DB 09
	MOV A,#$3C              ; E8 3C
	MOV Y,#$5D              ; 8D 5D
	CALL $09DB              ; 3F DB 09
	MOV A,#$F0              ; E8 F0
	MOV $F100,A             ; C5 F1 00
	MOV A,#$10              ; E8 10
	MOV $00FA,A             ; C5 FA 00
	MOV $53,A               ; C4 53: persist the initialization value.
	MOV A,#$01              ; E8 01
	MOV $00F1,A             ; C5 F1 00
	MOV A,$1B               ; E4 1B
	BNE $088B               ; D0 51

	; This timer/DSP-dispatch loop is reached in every captured Sound Test run.
	org $0840
SpcDspDispatchLoop:
	BCS $084A               ; B0 08: both outcomes occur in the live trace.
	CMP $4C,$4D             ; 69 4D 4C
	BNE $0858               ; D0 11
	BBS7 $4C,$0858          ; E3 4C 0E
	MOV A,$1562+Y           ; F6 62 15
	MOV $00F2,A             ; C5 F2 00
	MOV A,$156C+Y           ; F6 6C 15
	MOV X,A                 ; 5D
	MOV A,(X)               ; E6
	MOV $00F3,A             ; C5 F3 00
	DBNZ Y,$083C            ; FE E2: returns through the unlisted gate.

	org $0869
SpcTimerPoll:
	MOV Y,$00FD             ; EC FD 00: timer-0 counter read/reset.
	BEQ SpcTimerPoll        ; F0 FB
	PUSH Y                  ; 6D
	MOV A,#$20              ; E8 20
	MUL YA                  ; CF
	CLRC                    ; 60
	ADC A,$43               ; 84 43
	MOV $43,A               ; C4 43
	BCC SpcService          ; 90 07

	org $0880
SpcService:
	MOV A,$53               ; Periodic tick accumulator.
	POP Y                   ; EE: recover the timer-derived multiplier.
	MUL YA                  ; CF
	CLRC                    ; 60
	ADC A,$51               ; 84 51
	MOV $51,A               ; C4 51
	BCC SpcServiceFast
	CALL $0A4A
	MOV X,#$00
	CALL $08D8
	CALL $0BFC
	MOV X,#$01
	CALL $08D8
	CALL $0D26
	MOV X,#$03
	CALL $08D8
	BRA $0836

SpcServiceFast:
	MOV A,$04
	BEQ SpcVoiceScanDone
	MOV X,#$00
	MOV $47,#$01
SpcVoiceScan:
	MOV A,$31+X
	BEQ SpcVoiceNext
	CALL $148B
SpcVoiceNext:
	INC X
	INC X
	ASL $47
	MOV A,$47
	CMP A,#$40
	BNE SpcVoiceScan
SpcVoiceScanDone:
	MOV A,$05
	BEQ $08D5
	MOV X,#$0C
	MOV $47,#$40
	; The second voice scan mirrors the first; its exact exit is proven by
	; the trace, while the surrounding register semantics remain under study.
	JMP $0836

; Voice handlers reached by the live Sound Test traces. These are still
; byte-faithful analysis stubs until their DP-state contracts are closed.
	org $08A5
SpcCommandHandlerA:
	MOV A,$04
	BEQ $08BD
	MOV X,#$00
	MOV $47,#$01
	MOV A,$31+X
	BEQ $08B0
	CALL $148B
	INC X
	INC X
	ASL $47
	; Remaining target/loop state is retained below as raw bytes.
	DB $47

	org $08BF
SpcCommandHandlerB:
	MOV A,$05
	BEQ $08D3
	MOV X,#$0C
	MOV $47,#$40
	MOV A,$31+X
	BEQ $08D0
	CALL $148B
	INC X
	INC X
	ASL $47
	DB $47

org $08D5
SpcCommandHandlerC:
	DB $5F,$36,$08,$D5,$F4,$00,$F5,$F4,$00,$75,$F4,$00,$D0,$F8,$D4,$00
	DB $6F

; $15A1 is the live CPU-to-SPC handshake. The preceding $15A0 byte is not yet
; classified. The captured Sound Test command
; $01/$00 reaches the comparison at $15B4 (the port-read trace records the
; post-operand PC $15B7), then the command path runs through $15B7/$15BF.
; This proves the command protocol belongs to this driver block rather than
; to the generic standalone trace harness.
org $15A1
SpcPortHandshake:
	MOV A,#$BB              ; E8 BB
	MOV $00F5,A             ; C5 F5 00: upload/handshake acknowledgement.
SpcWaitCpuMarker:
	MOV A,$00F4             ; E5 F4 00
	CMP A,#$CC              ; 68 CC
	BNE SpcWaitCpuMarker    ; D0 F9
	BRA $15CF               ; 2F 20
SpcWaitPortChange:
	MOV Y,$00F4             ; EC F4 00
	BNE SpcWaitPortChange   ; D0 FB
	CMP Y,$00F4             ; 5E F4 00: live reads see the Sound Test ID.
	BNE $15C9               ; D0 0F
	MOV A,$00F5             ; E5 F5 00
	MOV $00F4,Y             ; CC F4 00
	MOV ($14)+Y,A           ; D7 14: write command byte through active pointer.
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
	BNE SpcWaitPortChange   ; D0 CD
	MOV X,#$31              ; CD 31
	MOV $00F1,X             ; C9 F1 00
	RET                     ; 6F

SpcWriteDspAddressY:
	MOV $00F2,Y             ; CC F2 00
	MOV A,$00F3             ; E5 F3 00
	RET                     ; 6F

; The event-stream byte reader. Every `sequence_streams` address in the
; capture report is a live ARAM read performed by the F7 at $13CB; the trace
; records its post-operand PC as $13CD. This establishes E0/E1/ED/E3 and note
; bytes as sequence data, not driver code.
org $13C7
SpcReadSequenceByte:
	MOVW $14,YA             ; DA 14: stream pointer supplied by voice state.
	MOV Y,#$00              ; 8D 00
	MOV A,[$14]+Y           ; F7 14: observed reads of each active stream.
	BEQ $13ED               ; F0 1E
	BMI $13D1               ; 30 07
	INC Y                   ; FC
	BMI $1414               ; 30 40
	MOV A,[$14]+Y           ; F7 14
	BPL $13CF               ; 10 F9
	CMP A,#$C8              ; 68 C8
	BEQ $141B               ; F0 3F
	CMP A,#$EF              ; 68 EF
	BEQ $1409               ; F0 29
	CMP A,#$E0              ; 68 E0
	BCC $1414               ; 90 30
	; Control-byte skip dispatcher. The $11FC+Y lookup produces the next
	; stream offset; traces of Sounds 01, 02, and 11 prove these advances:
	;   E0 -> +2 (one operand), E1 -> +2, ED -> +2, E3 -> +4 (three operands).
	; EF can change/rebase flow and is intentionally not assigned an operand
	; length yet.
	PUSH Y                  ; 6D
	MOV Y,A                 ; FD
	POP A                   ; AE
	ADC A,$11FC+Y           ; 96 FC 11
	MOV Y,A                 ; FD
	BRA SpcReadSequenceByte ; 2F DE

; The actual control-effect dispatcher is separate from the stream scanner.
; The live trace reaches this path with A equal to each stream control byte,
; then $101C resolves the byte to an effect routine through its driver table.
org $0B19
SpcDispatchSequenceControl:
	CMP A,#$E0              ; 68 E0
	BCC $0B22               ; 90 05: non-control path.
	CALL SpcResolveSequenceControl ; 3F 1C 10
	BRA $0ADB               ; 2F B9

; Observed handler entries from real E0/E1/E3/ED/EF event execution:
;   E0 -> $1038: one operand; populates per-voice instrument/program state.
;   E1 -> $1091: one operand; writes a masked five-bit pan parameter.
;   E3 -> $10B8: delay, depth, rate; configures per-voice vibrato.
;   ED -> $1141: one operand; writes a per-voice volume level.
;   EF -> $1167: target-lo, target-hi, count; rebases to that stream target.
; Sound Test state-bus traces close E0/E1/ED downstream ownership: E0 writes
; SRCN and ADSR descriptor fields through $F2/$F3; E1 selects the left/right
; split; ED supplies the scalar volume level. E3's delay/depth/rate state is
; consumed at $143B-$1482 and writes the DSP pitch registers through $09DE.
org $101C
SpcResolveSequenceControl:
	ASL A                   ; 1C
	MOV Y,A                 ; FD
	MOV A,$11DF+Y           ; F6 DF 11: handler return-table byte.
	PUSH A                  ; 2D
	MOV A,$11DE+Y           ; F6 DE 11
	PUSH A                  ; 2D
	MOV A,Y                 ; DD
	LSR A                   ; 5C
	MOV Y,A                 ; FD
	MOV A,$127C+Y           ; F6 7C 12: operand-count/stream advance table.
	BEQ $102E               ; F0 08
	DB $E7,$30,$BB,$30,$D0,$02,$BB,$31,$FD,$6F

org $1038
SpcSequenceE0Instrument:
	DB $D5,$11,$02,$FD,$10,$06,$80,$A8,$CA,$60,$84,$5F
	; Live E0 $06 execution selects the same instrument/source family later
	; observed at DSP KON, but the descriptor-table field names are pending.

org $1091
SpcSequenceE1Pan:
	DB $D5,$51,$03,$28,$1F,$D5,$31,$03,$E8,$00,$D5,$30,$03,$6F

org $10B8
SpcSequenceE3Vibrato:
	DB $D5,$B0,$02,$3F,$2E,$10,$D5,$A1,$02,$3F,$2E,$10
	DB $D4,$B1,$D5,$C1,$02,$E8,$00,$D5,$B1,$02,$6F

org $1141
SpcSequenceEDVolume:
	DB $D5,$01,$03,$E8,$00,$D5,$00,$03,$6F

org $1167
SpcSequenceEFCall:
	DB $D5,$40,$02,$3F,$2E,$10,$D5,$41,$02,$3F,$2E,$10
	DB $D4,$80,$F4,$30,$D5,$30,$02,$F4,$31,$D5,$31,$02
	DB $F5,$40,$02,$D4,$30,$F5,$41,$02,$D4,$31,$6F

; Additional one-operand controls observed in the live command/sequence
; reader. Their exact direct-state effects are proven; their format-level
; names remain intentionally unspecified.
org $10F6
SpcControlE7ClearWorkWord:
	MOV A,#$00              ; E8 00
	MOVW $52,YA             ; DA 52
	RET                     ; 6F

org $122D
SpcControlFASetDirect5F:
	MOV $5F,A               ; C4 5F
	RET                     ; 6F

; Per-voice vibrato service, reached from the fast voice scan. The state-bus
; trace proves the values produced here proceed through $0950/$09DE to the
; DSP pitch low/high pairs. X is the two-byte voice index.
org $143B
SpcServiceVibrato:
	MOV A,$02B0+X          ; F5 B0 02: configured start delay.
	CBNE $B0+X,$1485       ; DE B0 44: increment delay counter until ready.
	MOV A,$0100+X          ; F5 00 01: current voice pitch state.
	CMP A,$02B1+X          ; 75 B1 02
	BNE $144E              ; D0 05
	MOV A,$02C1+X          ; F5 C1 02: reload configured rate.
	BRA $145B              ; 2F 0D
	SETP                    ; 40: direct-page working counter page.
	INC $00+X               ; BB 00
	CLRP                    ; 20
	MOV Y,A                 ; FD
	BEQ $1457               ; F0 02
	MOV A,$B1+X             ; F4 B1
	CLRC                    ; 60
	ADC A,$02C0+X           ; 95 C0 02
	MOV $B1+X,A             ; D4 B1: live rate phase.
	MOV A,$02A0+X           ; F5 A0 02: pitch modulation accumulator.
	CLRC                    ; 60
	ADC A,$02A1+X           ; 95 A1 02: configured depth.
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
	JMP $0950               ; 5F 50 09: applies computed pitch via $09DE.
	INC $B0+X               ; BB B0: wait another tick while delay is active.
	BBS7 $13,$1482          ; E3 13 F8
	RET                     ; 6F
