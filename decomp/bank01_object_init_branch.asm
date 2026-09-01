; Continue filling object records when the source count is non-zero.

	BNE ObjectInitFill

; These declarations anchor control-flow targets that will be promoted with
; their bodies later; they emit no ROM bytes in this seed.
