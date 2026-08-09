#ifndef _CC_H
#define _CC_H

// Basic C-subset compiler: lexes/parses a small C program into an AST,
// then emits real x86-32 machine code into a static buffer and returns
// it so the caller can execute it directly as a function pointer. This
// works with zero new kernel infrastructure because gdt.c's code segment
// (selector 0x08) is already a flat 4GB ring-0 executable segment with no
// paging/NX -- any address in the linear space, including this .bss
// buffer, is already executable.
//
// Supported subset: int locals, arithmetic (+ - * /, unary -, parens),
// comparisons (== != < > <= >=, non-chaining), if/else, while, a single
// non-standard builtin statement print(expr);, return expr;, and exactly
// one function: int main() { ... }. No user functions besides main, no
// arrays/pointers/structs/strings/globals/preprocessor -- future work.

#define CC_CODE_SIZE   (32 * 1024)  // .bss buffer for generated machine code
#define CC_ARENA_SIZE  (64 * 1024)  // .bss bump arena for AST nodes
#define CC_MAX_LOCALS  32            // keeps every local's [ebp-disp] within
                                      // disp8 range (-128..-4), see cc.c

// Compiles `src` (src_len bytes, need not be NUL-terminated). print_fn is
// the address of a real, statically-linked C function of signature
// void(int) -- cc.c bakes this pointer directly into the generated code
// as the call target for every print(expr); statement.
//
// On success: returns the generated code length (>=0) and sets *out_code
// to the internal code buffer (valid until the next cc_compile() call --
// same one-shot-buffer contract as fs.c's g_copy_buf). Caller casts
// *out_code to int(*)(void) and calls it directly.
//
// On failure: returns -1, fills err_msg (ASCII, no trailing newline,
// truncated to err_msg_max-1 and NUL-terminated) and *err_line (1-based
// source line, or -1 if not line-specific).
int cc_compile(const char *src, int src_len, void (*print_fn)(int),
                unsigned char **out_code,
                char *err_msg, int err_msg_max, int *err_line);

// On-disk format for a compiled program (see cc.c): 4-byte 'R''K''X''C'
// magic, 4-byte little-endian code_len, then code_len bytes of machine
// code -- a raw dump of what cc_compile() generated. No relocation is
// needed: internal if/while jumps are buffer-relative (safe under
// whole-buffer copy), and the only absolute address baked into the code
// is print_fn's, which points at fixed kernel code, not at the buffer
// itself.
#define CC_PROGRAM_HEADER_SIZE 8

// Packs `code` (code_len bytes, as returned by cc_compile()) into
// out_buf. Returns total bytes written (CC_PROGRAM_HEADER_SIZE +
// code_len), or -1 if that wouldn't fit in out_buf_max -- callers use
// this to detect "compiled program too big for a file" before ever
// touching the filesystem.
int cc_save_program(unsigned char *out_buf, int out_buf_max,
                     const unsigned char *code, int code_len);

// Validates file_bytes (file_len bytes, as read back from disk) as a
// cc_save_program()-produced file: checks the magic and that the
// declared code_len exactly accounts for the rest of file_len. On
// success, copies the code portion into cc_compile()'s own internal
// buffer (re-serialized for the same self-modifying-code reason
// cc_compile() already handles), sets *out_code to it (same
// valid-until-next-call contract as cc_compile()'s out_code) and
// returns code_len. On failure returns -1 and sets *out_code = 0 --
// callers must not execute anything in that case.
int cc_load_program(const unsigned char *file_bytes, int file_len,
                     unsigned char **out_code);

#endif
