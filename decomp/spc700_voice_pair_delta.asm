; Shared voice-pair delta helper at SPC ARAM $1294.
; The common voice-service tail calls it from both pitch/mix update paths.
; When bit 7 of `$12` is clear, the helper exits without a delta. Otherwise,
; `$0E/$0F` is the current pair and `$10/$11` the selected target/work pair;
; the resulting signed delta is left in YA.

SpcVoicePairDelta:
	BBC7 $12,$129D          ; F3 12 06
	MOVW $14,YA             ; DA 14
	MOVW YA,$0E             ; BA 0E
	SUBW YA,$14             ; 9A 14
	RET                     ; 6F
