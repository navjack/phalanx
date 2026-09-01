; SoundStart indexes this four-byte table with command<<2.  The three
; observed records are parameter triplets used to seed SPC command $10-$12;
; the fourth byte is reserved/padding in each record.
org $1F8AC7
SoundCommandParameters:
  db $00,$00,$00,$00
  db $00,$80,$1D,$00
  db $00,$80,$1E,$00
assert pc() == $1F8AD3
