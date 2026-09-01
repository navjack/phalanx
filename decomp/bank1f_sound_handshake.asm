; Continuation of SoundStart: clear/prime the APU command handshake, wait for
; VBlank through the existing $1F:8A64 helper, then finish with RTL.
org $1F8938
  JSL $008340
  JSL $008340
  JSL $008340
  LDA.b #$FF
  STA.w $2140
  JSL $008340
  JSL $008340
  JSL $008340
  JSL $008340
  STZ.w $4200
  JSR.w $8A64
  LDA.w $4210
  LDA.b #$81
  STA.w $4200
  REP #$20
  AND.w #$00FF
  SEP #$20
  RTL
assert pc() == $1F896F
