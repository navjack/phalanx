; Upload the first pending VRAM page when its bit is set in the request mask.

UploadPage81C4:
	LDA.w $02B3
	AND.b #$08
	BEQ UploadCallerReturn
	JSL $0082F1
	JSL $0080D5
	LDA.w $02A9
	STA.w $2117
	LDA.w $02AE
	ASL A
	STA.w $4306
	BRA UploadDMATail
