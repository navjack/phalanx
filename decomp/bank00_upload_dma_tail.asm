; Common tail for the small VRAM upload helpers. The caller has already
; selected the source page and length in DMA channel 0.

UploadDMATail:
	STZ.w $2116
	STZ.w $4305
	LDX.w #$1809
	STX.w $4300
	LDX.w #$825A
	STX.w $4302
	LDA.b #$00
	STA.w $4304
	LDA.b #$01
	STA.w $420B
	JSL $0080E1

UploadDMATailReturn:
	RTL
