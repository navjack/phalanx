; $1F:8B9F — sound consumer prologue.
; Entry width state is inherited from callers, so the original instruction
; bytes are retained explicitly here.  The sequence calls shared event
; helpers, establishes DB=$1F, clears $7E:2C00, updates $2100/$4200, and RTLs.
org $1F8B9F
  db $22,$4F,$8E,$1F ; JSL $1F:8E4F
  db $AB,$8B         ; PLB : PHB
  db $A9,$1F         ; LDA #$1F (M=8 at entry)
  db $48,$AB         ; PHA : PLB
  db $22,$17,$EB,$1F ; JSL $1F:EB17
  db $AB             ; PLB
  db $A9,$00,$8F,$00,$2C,$7E ; LDA #$00 : STA.l $7E:2C00
  db $A9,$80,$8D,$00,$21     ; LDA #$80 : STA $2100
  db $A9,$01,$8D,$00,$42     ; LDA #$01 : STA $4200
  db $6B             ; RTL
assert pc() == $1F8BBF
