; Two-byte stream reader at SPC ARAM $09E2.
; It reads and post-increments the `$40/$41` pointer twice, returning the
; first byte in A and second byte in Y. Live preview traces reach every byte.

SpcReadStreamWord:
	MOV Y,#$00              ; 8D 00
	MOV A,($40)+Y           ; F7 40
	INCW $40                ; 3A 40
	PUSH A                  ; 2D
	MOV A,($40)+Y           ; F7 40
	INCW $40                ; 3A 40
	MOV Y,A                 ; FD
	POP A                   ; AE
	RET                     ; 6F
