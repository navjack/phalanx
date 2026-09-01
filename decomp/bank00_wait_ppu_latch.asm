; Wait for the short PPU latch/countdown to clear before returning.

WaitPPULatch:
	LDA.b #$01
	STA.w $023C
WaitPPULoop:
	LDA.w $023C
	BNE WaitPPULoop
	RTL
