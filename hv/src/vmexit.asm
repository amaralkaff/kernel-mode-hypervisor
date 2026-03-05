extern vmentry_handler_cpp:proc
PUBLIC vmexit_handler
extern RtlCaptureContext:proc
.code

vmexit_handler proc
pushfq
push r15
push r14
push r13
push r12
push r11
push r10
push r9
push r8
push rdi
push rsi
push rbp
push rbx
push rdx
push rcx
push rax

mov rcx, rsp
sub rsp, 28h
call vmentry_handler_cpp
add rsp, 28h

; rax = 0 means normal vmresume
; rax != 0 means devirt (rax = guest_rsp)
test rax, rax
jnz devirt

; normal path: restore regs and vmresume
pop rax
pop rcx
pop rdx
pop rbx
pop rbp
pop rsi
pop rdi
pop r8
pop r9
pop r10
pop r11
pop r12
pop r13
pop r14
pop r15
popfq
vmresume

; vmresume failed
int 3
jmp $

devirt:
; rax = guest RSP
; regs->rcx = guest RFLAGS, regs->rdx = guest RIP
mov r11, rax
mov rcx, [rsp + 10h]    ; guest RIP (regs->rdx offset)
mov rdx, [rsp + 08h]    ; guest RFLAGS (regs->rcx offset)

vmxoff

; restore callee-saved from guest state
mov rbx, [rsp + 18h]
mov rbp, [rsp + 20h]
mov rsi, [rsp + 28h]
mov rdi, [rsp + 30h]
mov r12, [rsp + 58h]
mov r13, [rsp + 60h]
mov r14, [rsp + 68h]
mov r15, [rsp + 70h]

; switch to guest stack
mov rsp, r11

; restore guest RFLAGS
push rdx
popfq

mov rax, 1
jmp rcx

vmexit_handler endp

END
