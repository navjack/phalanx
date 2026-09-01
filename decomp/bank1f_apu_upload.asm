; $1F:8A64 — transfer one length-prefixed block from the long pointer in
; direct-page $10-$12 into SPC ARAM. The actual Sound Test source-read trace
; proves this is the producer of the stable driver upload from $1E:B242.
;
; Entry: $10-$12 = 24-bit ROM source record; X/Y are scratch. The record is
; [length:16][SPC destination:16][payload:length]. The IPL protocol echoes an
; incrementing byte counter on $2140 and accepts payload via $2141.
org $1F8A64
APUUploadBlock:
  PHP
  REP #$30
  LDY.w #$0000
  LDA.w #$BBAA
APUWaitReady:
  CMP.w $2140
  BNE APUWaitReady
  SEP #$20
  LDA.b #$CC
  BRA APUReadBlockHeader

APUSendByte:
  LDA [$10],Y
  INY
  XBA
  LDA.b #$00
  BRA APUSendCounter
APUWaitCounter:
  XBA
  LDA [$10],Y
  INY
  XBA
APUWaitEcho:
  CMP.w $2140
  BNE APUWaitEcho
  INC A
APUSendCounter:
  REP #$20
  STA.w $2140
  SEP #$20
  DEX
  BNE APUWaitCounter
APUWaitCompletion:
  CMP.w $2140
  BNE APUWaitCompletion
APUAdvanceCounter:
  ADC.b #$03
  BEQ APUAdvanceCounter

APUReadBlockHeader:
  PHA
  REP #$20
  LDA [$10],Y             ; 16-bit payload length.
  INY
  INY
  TAX
  LDA [$10],Y             ; 16-bit SPC destination.
  INY
  INY
  STA.w $2142
  SEP #$20
  CPX.w #$0001
  LDA.b #$00
  ROL A
  STA.w $2141
  ADC.b #$7F
  PLA
  STA.w $2140
APUWaitHeaderEcho:
  CMP.w $2140
  BNE APUWaitHeaderEcho
  BVS APUSendByte
  PLP
  RTS
assert pc() == $1F8AC7
