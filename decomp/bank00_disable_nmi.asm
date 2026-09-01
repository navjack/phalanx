; Disable NMI while updating the initialization state.

	LDA $0275
	AND #$7F
	STA $0275
	STA $4200
	RTL
