; Upload the second pending VRAM page when its bit is set in the request mask.

UploadPage81E2:
	LDA.w $02B3
	AND.b #$04
	BEQ UploadCallerReturn
	JSL $0082F1
	JSL $0080D5
	LDA.w $02AA
	STA.w $2117
	LDA.w $02AF
	ASL A
	STA.w $4306
	BRA UploadDMATail
