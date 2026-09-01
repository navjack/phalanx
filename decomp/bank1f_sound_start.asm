; $1F:8909 — begin sound/APU command setup (entry M=1, X=0).
; Caller supplies an 8-bit command in A.  The four-byte record at $1F:8AC7
; expands it into the SPC-side parameter bytes at direct-page $10-$12.
; This source intentionally stops at the first JSL helper boundary; the
; following handshake loop remains an identified code range for conversion.

org $1F8909
SoundStart:
  REP #$20
  AND.w #$00FF
  SEP #$20
  STA.b $DE
  STA.b $D4
  ASL A
  ASL A
  TAX
  LDA.l $1F8AC7,X
  STA.b $10
  INX
  LDA.l $1F8AC7,X
  STA.b $11
  INX
  LDA.l $1F8AC7,X
  STA.b $12
  STZ.w $2140
  STZ.w $2141
  STZ.w $2142
  JSL $008340

assert pc() == $1F8938
