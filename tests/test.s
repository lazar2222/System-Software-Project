.section ivt
	.word _start
	.word _error
	.word _timer
	.word _term
	.skip 8
.section isr
_error:
	ldr r0, $69
	str r0, 0xFF00
	halt
_term:
	ldr r0, 0xFF02
	ldr r1, $32
	cmp r0, r1
	jeq next
	ldr r1, $48
	sub r0, r1
	ldr r1, $10
	mul r2,r1
	add r2,r0
	iret
next:
	add r4,r2
	iret
_timer:
	ldr r0, $0
	cmp r0, r4
	jeq return
	ldr r1, r4
	ldr r0, $10
	div r4, r0
	mul r4, r0
	sub r1, r4
	div r4, r0
	ldr r0, $48
	add r1,r0
	str r1, 0xFF00
	jmp _timer
return: halt
_start:
	ldr r0, $0x7
    str r0, 0xFF10
	inf: jmp inf
.end