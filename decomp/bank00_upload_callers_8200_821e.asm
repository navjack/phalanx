; Two adjacent request-mask driven VRAM upload callers.

UploadPage8200:
	LDA.w $02B3
	AND.b #$02
	BEQ UploadDMATailReturn
	JSL $0082F1
	JSL $0080D5
	LDA.w $02AB
	STA.w $2117
	LDA.w $02B0
	ASL A
	STA.w $4306
	BRA UploadDMATail

UploadPage821E:
	LDA.w $02B3
	LSR A
	BCC UploadDMATailReturn
	JSL $0082F1
	JSL $0080D5
	LDA.w $02AC
	STA.w $2117
	LDA.w $02B1
	ASL A
	STA.w $4306
	BRA UploadDMATail
