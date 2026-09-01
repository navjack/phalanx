; Clear the four 0x80-byte sprite attribute tables and seed the small
; metadata table used by the object system.

ClearSpriteTables:
	PHX
	LDA.b #$40
	LDX.w #$007C
ClearSpriteLoop:
	STA.w $02E0,X
	STA.w $0360,X
	STA.w $03E0,X
	STA.w $0460,X
	DEX
	DEX
	DEX
	DEX
	BPL ClearSpriteLoop
	LDX.w #$001F
	LDA.b #$55
SeedSpriteMetadata:
	STA.w $04E0,X
	DEX
	BPL SeedSpriteMetadata
	STZ $4E
	PLX

UploadCallerReturn:
	RTL
