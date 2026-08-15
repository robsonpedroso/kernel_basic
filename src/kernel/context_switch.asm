[bits 32]
global context_switch
global thread_trampoline
extern thread_exit

; void context_switch(unsigned int *old_esp_store, unsigned int new_esp)
; Saves the calling thread's callee-saved registers on its own stack,
; records where it ended up (*old_esp_store = esp), then switches to
; new_esp and pops the same 4 registers back -- either a previously parked
; thread's own saved values, or the frame thread_create() fabricated for a
; thread that has never run yet (see thread_trampoline below). Works for
; both a voluntary call (thread_yield/thread_block) and one made from
; inside timer_irq_handler (preemption): either way, the thread being
; switched away from simply resumes right after its own "call
; context_switch" the next time it is picked -- an IRQ-driven switch
; resumes inside the handler, which then returns normally through
; irq_common_stub's iret, restoring that thread's own interrupt frame.
;
; Deliberately does NOT save/restore eflags (no pushfd/popfd). IF is owned
; exclusively by preempt_disable/enable's explicit cli/sti (thread.c) and,
; for a thread caught mid-IRQ by scheduler_tick, by that thread's own
; interrupt frame and eventual iret in isr.asm. Adding a third source of
; truth for IF here would let a freshly fabricated thread's flags -- or a
; stale snapshot from a thread that was suspended a different way -- flip
; interrupts on in the middle of somebody else's critical section.
context_switch:
    mov eax, [esp + 4]
    mov edx, [esp + 8]
    push ebp
    push ebx
    push esi
    push edi
    mov [eax], esp
    mov esp, edx
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; Landing pad for a thread's first-ever context_switch: thread_create()
; fabricates a stack frame so the "pop esi" / "pop ebx" above load the
; entry point and argument here instead of a real saved register.
;
; Every other thread resumption restores IF one of two ways: a voluntary
; switch resumes into code that itself calls preempt_enable() shortly
; after, and a preemptive one resumes inside timer_irq_handler, which
; unwinds through irq_common_stub's iret and restores that thread's own
; real interrupt frame. A brand new thread has neither -- it lands here
; straight from context_switch's plain "ret" with whatever IF the switch
; happened to run with (0, if -- as is typical -- it was scheduled in from
; inside the timer IRQ), and nothing would ever turn interrupts back on
; for it otherwise. So: sti unconditionally before running any thread code.
thread_trampoline:
    sti
    push ebx        ; arg
    call esi        ; entry(arg)
    add esp, 4
    call thread_exit
.hang:
    hlt
    jmp .hang
