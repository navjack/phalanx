; $1F:896F — alternate sound-command setup (entry M=1, X=0).
; This is the same APU launch shape as $1F:8909, but uses the second
; parameter table at $1F:8AD3 and preserves its command in $DF/$D6.
org $1F896F
  REP #$20
  AND.w #$00FF
  SEP #$20
  STA.b $DF
  STA.b $D6
  ASL A
  ASL A
  TAX
  LDA.l $1F8AD3,X
  STA.b $10
  INX
  LDA.l $1F8AD3,X
  STA.b $11
  INX
  LDA.l $1F8AD3,X
  STA.b $12
  STZ.w $2140
  STZ.w $2141
  STZ.w $2142
  JSL $008340
assert pc() == $1F899E
