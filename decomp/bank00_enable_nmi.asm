; Enable NMI after the initialization state is ready.

	LDA $0275
	ORA #$80
	STA $0275
	STA $4200
	RTL
