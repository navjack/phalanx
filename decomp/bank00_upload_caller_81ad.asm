; Upload the initial VRAM page. This path is unconditional once reached.

UploadPage81AD:
	JSL $0082F1
	JSL $0080D5
	LDA.w $02AD
	STA.w $2117
	LDA.w $02B2
	ASL A
	STA.w $4306
	BRA UploadDMATail
