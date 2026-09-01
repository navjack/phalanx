; Completion of the $1F:896F command path.  The sequence is deliberately
; explicit: each JSL is an observable APU handshake step in the original ROM.
org $1F899E
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
assert pc() == $1F89D5
