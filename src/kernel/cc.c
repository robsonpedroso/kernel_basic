#include "../include/cc.h"
#include "../include/string.h"

// Basic C-subset compiler -- lexer, arena-allocated AST, recursive
// descent parser, and a naive x86-32 stack-machine codegen that hand
// -encodes machine code bytes directly into a static buffer. See cc.h
// for the supported subset and the "why does calling a .bss buffer as
// code even work" explanation (gdt.c's flat 4GB ring-0 code segment).
//
// Everything here is static/file-local: cc_compile() is the only public
// entry point. All state (lexer position, arena, symbol table, code
// buffer, error latch) is reset at the top of every cc_compile() call,
// so this is safe to call again for a new source after a previous run
// (success or failure) -- same one-shot-buffer contract as fs.c's
// g_copy_buf.

// ---- error latch ----
// No exceptions/setjmp in this freestanding kernel: the first error sets
// this latch and every parse/codegen function checks it immediately
// after any sub-call that could fail, turning the rest of the pass into
// a cheap no-op instead of touching a half-built AST or a partial
// codegen buffer.
static int  g_cc_error;
static char g_cc_err_msg[64];
static int  g_cc_err_line;

static void cc_error(int line, const char *msg) {
	if (g_cc_error) {
		return; // keep only the first error
	}
	g_cc_error = 1;
	g_cc_err_line = line;
	int i = 0;
	while (msg[i] && i < 63) {
		g_cc_err_msg[i] = msg[i];
		i++;
	}
	g_cc_err_msg[i] = 0;
}

// ---- character classification (no ctype.h in this project) ----
static int cc_is_digit(char c) { return c >= '0' && c <= '9'; }
static int cc_is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static int cc_is_alnum(char c) { return cc_is_alpha(c) || cc_is_digit(c); }

static void cc_copy_text(char *dst, char *src) {
	int i = 0;
	while (src[i] && i < 31) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
}

// ---- lexer ----
typedef enum {
	TOK_EOF, TOK_IDENT, TOK_NUM,
	TOK_KW_INT, TOK_KW_IF, TOK_KW_ELSE, TOK_KW_WHILE, TOK_KW_PRINT, TOK_KW_RETURN,
	TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN, TOK_SEMI,
	TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_ASSIGN,
	TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
} cc_tok_type_t;

typedef struct {
	cc_tok_type_t type;
	int int_val;   // TOK_NUM
	char text[32]; // TOK_IDENT
	int line;
} cc_token_t;

static const char *g_src;
static int g_src_len;
static int g_pos;
static int g_line;

static void cc_skip_ws(void) {
	while (g_pos < g_src_len) {
		char c = g_src[g_pos];
		if (c == '\n') {
			g_line++;
			g_pos++;
		} else if (c == ' ' || c == '\t' || c == '\r') {
			g_pos++;
		} else {
			break;
		}
	}
}

static void cc_lex_next(cc_token_t *out) {
	cc_skip_ws();
	out->line = g_line;
	out->text[0] = 0;
	out->int_val = 0;

	if (g_pos >= g_src_len) {
		out->type = TOK_EOF;
		return;
	}

	char c = g_src[g_pos];

	if (cc_is_digit(c)) {
		int val = 0;
		while (g_pos < g_src_len && cc_is_digit(g_src[g_pos])) {
			val = val * 10 + (g_src[g_pos] - '0');
			g_pos++;
		}
		out->type = TOK_NUM;
		out->int_val = val;
		return;
	}

	if (cc_is_alpha(c)) {
		int i = 0;
		while (g_pos < g_src_len && cc_is_alnum(g_src[g_pos])) {
			if (i < 31) {
				out->text[i++] = g_src[g_pos];
			}
			g_pos++;
		}
		out->text[i] = 0;
		if (strcmp(out->text, "int") == 0) out->type = TOK_KW_INT;
		else if (strcmp(out->text, "if") == 0) out->type = TOK_KW_IF;
		else if (strcmp(out->text, "else") == 0) out->type = TOK_KW_ELSE;
		else if (strcmp(out->text, "while") == 0) out->type = TOK_KW_WHILE;
		else if (strcmp(out->text, "print") == 0) out->type = TOK_KW_PRINT;
		else if (strcmp(out->text, "return") == 0) out->type = TOK_KW_RETURN;
		else out->type = TOK_IDENT;
		return;
	}

	g_pos++; // consume c -- two-char operators below advance g_pos once more
	switch (c) {
		case '{': out->type = TOK_LBRACE; return;
		case '}': out->type = TOK_RBRACE; return;
		case '(': out->type = TOK_LPAREN; return;
		case ')': out->type = TOK_RPAREN; return;
		case ';': out->type = TOK_SEMI; return;
		case '+': out->type = TOK_PLUS; return;
		case '-': out->type = TOK_MINUS; return;
		case '*': out->type = TOK_STAR; return;
		case '/': out->type = TOK_SLASH; return;
		case '=':
			if (g_pos < g_src_len && g_src[g_pos] == '=') { g_pos++; out->type = TOK_EQ; }
			else out->type = TOK_ASSIGN;
			return;
		case '!':
			if (g_pos < g_src_len && g_src[g_pos] == '=') { g_pos++; out->type = TOK_NE; }
			else { cc_error(out->line, "caractere invalido"); out->type = TOK_EOF; }
			return;
		case '<':
			if (g_pos < g_src_len && g_src[g_pos] == '=') { g_pos++; out->type = TOK_LE; }
			else out->type = TOK_LT;
			return;
		case '>':
			if (g_pos < g_src_len && g_src[g_pos] == '=') { g_pos++; out->type = TOK_GE; }
			else out->type = TOK_GT;
			return;
		default:
			cc_error(out->line, "caractere invalido");
			out->type = TOK_EOF;
			return;
	}
}

// ---- AST arena ----
// Bump allocator, reset per cc_compile() call -- same reasoning fs.h
// gives for not bothering with a free-space map at this scale: nothing
// here ever needs individual nodes freed, an abandoned arena on a parse
// error is the entire "cleanup" story.
static char g_cc_arena[CC_ARENA_SIZE];
static int  g_cc_arena_pos;

static void *cc_arena_alloc(int size) {
	if (g_cc_error) {
		return 0;
	}
	size = (size + 3) & ~3;
	if (g_cc_arena_pos + size > CC_ARENA_SIZE) {
		cc_error(-1, "programa fonte grande demais");
		return 0;
	}
	void *p = &g_cc_arena[g_cc_arena_pos];
	g_cc_arena_pos += size;
	return p;
}

typedef enum {
	ND_NUM, ND_VAR, ND_UNM, ND_BINOP, ND_ASSIGN, ND_DECL,
	ND_IF, ND_WHILE, ND_PRINT, ND_RETURN, ND_BLOCK
} cc_node_kind_t;

// a/b/c are kind-specific children (see the usage comment on each case
// below); `next` is a SEPARATE field used only to chain sibling
// statements inside an ND_BLOCK -- if/while already use b for their own
// child, so block-chaining cannot reuse a/b/c without colliding.
typedef struct cc_node {
	cc_node_kind_t kind;
	int line;
	int int_val;         // ND_NUM
	int var_offset;       // ND_VAR / ND_ASSIGN / ND_DECL: [ebp-var_offset]
	cc_tok_type_t op;      // ND_BINOP
	struct cc_node *a, *b, *c;
	// ND_UNM: a=operand.  ND_BINOP: a=left,b=right.
	// ND_ASSIGN/ND_DECL: a=rhs expr (DECL: 0 if no initializer).
	// ND_IF: a=cond,b=then,c=else-or-0.  ND_WHILE: a=cond,b=body.
	// ND_PRINT/ND_RETURN: a=expr.  ND_BLOCK: a=first stmt (see next).
	struct cc_node *next;   // next statement in the same block, or 0
} cc_node_t;

// ---- symbol table (flat, main()-only scope, no shadowing) ----
typedef struct { char name[32]; int offset; } cc_sym_t;
static cc_sym_t g_cc_syms[CC_MAX_LOCALS];
static int g_cc_sym_count;

static int cc_sym_declare(char *name, int line) {
	for (int i = 0; i < g_cc_sym_count; i++) {
		if (strcmp(g_cc_syms[i].name, name) == 0) {
			cc_error(line, "variavel redeclarada");
			return -1;
		}
	}
	if (g_cc_sym_count >= CC_MAX_LOCALS) {
		cc_error(line, "muitas variaveis locais");
		return -1;
	}
	cc_copy_text(g_cc_syms[g_cc_sym_count].name, name);
	int offset = 4 * (g_cc_sym_count + 1);
	g_cc_syms[g_cc_sym_count].offset = offset;
	g_cc_sym_count++;
	return offset;
}

static int cc_sym_lookup(char *name) {
	for (int i = 0; i < g_cc_sym_count; i++) {
		if (strcmp(g_cc_syms[i].name, name) == 0) {
			return g_cc_syms[i].offset;
		}
	}
	return -1;
}

// ---- parser (recursive descent, 1 token of lookahead) ----
static cc_token_t g_cur;

static void cc_advance(void) {
	if (g_cc_error) {
		return;
	}
	cc_lex_next(&g_cur);
}

static void cc_expect(cc_tok_type_t t, const char *msg) {
	if (g_cc_error) {
		return;
	}
	if (g_cur.type != t) {
		cc_error(g_cur.line, msg);
		return;
	}
	cc_advance();
}

// Forward declarations -- expr/stmt/block are mutually recursive
// (parenthesized expressions parse a nested expr; blocks parse stmts
// which can themselves be blocks or contain if/while bodies that are
// stmts again).
static cc_node_t *cc_parse_expr(void);
static cc_node_t *cc_parse_stmt(void);
static cc_node_t *cc_parse_block(void);

static cc_node_t *cc_new_binop(cc_tok_type_t op, cc_node_t *a, cc_node_t *b, int line) {
	cc_node_t *n = cc_arena_alloc(sizeof(cc_node_t));
	if (!n) {
		return 0;
	}
	n->kind = ND_BINOP;
	n->op = op;
	n->a = a;
	n->b = b;
	n->c = 0;
	n->next = 0;
	n->line = line;
	return n;
}

static cc_node_t *cc_parse_primary(void) {
	if (g_cc_error) {
		return 0;
	}
	int line = g_cur.line;

	if (g_cur.type == TOK_NUM) {
		cc_node_t *n = cc_arena_alloc(sizeof(cc_node_t));
		if (!n) return 0;
		n->kind = ND_NUM;
		n->int_val = g_cur.int_val;
		n->line = line;
		n->next = 0;
		cc_advance();
		return n;
	}
	if (g_cur.type == TOK_IDENT) {
		char name[32];
		cc_copy_text(name, g_cur.text);
		int offset = cc_sym_lookup(name);
		if (offset < 0) {
			cc_error(line, "variavel nao declarada");
			return 0;
		}
		cc_advance();
		cc_node_t *n = cc_arena_alloc(sizeof(cc_node_t));
		if (!n) return 0;
		n->kind = ND_VAR;
		n->var_offset = offset;
		n->line = line;
		n->next = 0;
		return n;
	}
	if (g_cur.type == TOK_LPAREN) {
		cc_advance();
		cc_node_t *n = cc_parse_expr();
		cc_expect(TOK_RPAREN, "esperado ')'");
		return n;
	}
	cc_error(line, "esperado numero, variavel ou '('");
	return 0;
}

static cc_node_t *cc_parse_unary(void) {
	if (g_cc_error) {
		return 0;
	}
	int line = g_cur.line;
	if (g_cur.type == TOK_MINUS) {
		cc_advance();
		cc_node_t *operand = cc_parse_unary();
		if (g_cc_error) return 0;
		cc_node_t *n = cc_arena_alloc(sizeof(cc_node_t));
		if (!n) return 0;
		n->kind = ND_UNM;
		n->a = operand;
		n->line = line;
		n->next = 0;
		return n;
	}
	return cc_parse_primary();
}

static cc_node_t *cc_parse_term(void) {
	cc_node_t *n = cc_parse_unary();
	while (!g_cc_error && (g_cur.type == TOK_STAR || g_cur.type == TOK_SLASH)) {
		cc_tok_type_t op = g_cur.type;
		int line = g_cur.line;
		cc_advance();
		cc_node_t *rhs = cc_parse_unary();
		n = cc_new_binop(op, n, rhs, line);
	}
	return n;
}

static cc_node_t *cc_parse_additive(void) {
	cc_node_t *n = cc_parse_term();
	while (!g_cc_error && (g_cur.type == TOK_PLUS || g_cur.type == TOK_MINUS)) {
		cc_tok_type_t op = g_cur.type;
		int line = g_cur.line;
		cc_advance();
		cc_node_t *rhs = cc_parse_term();
		n = cc_new_binop(op, n, rhs, line);
	}
	return n;
}

static int cc_is_cmp_tok(cc_tok_type_t t) {
	return t == TOK_EQ || t == TOK_NE || t == TOK_LT || t == TOK_GT || t == TOK_LE || t == TOK_GE;
}

static cc_node_t *cc_parse_comparison(void) {
	cc_node_t *n = cc_parse_additive();
	if (!g_cc_error && cc_is_cmp_tok(g_cur.type)) {
		// At most one comparison operator -- "a < b < c" is a syntax
		// error here, not chained comparison semantics nobody asked for.
		cc_tok_type_t op = g_cur.type;
		int line = g_cur.line;
		cc_advance();
		cc_node_t *rhs = cc_parse_additive();
		n = cc_new_binop(op, n, rhs, line);
	}
	return n;
}

static cc_node_t *cc_parse_expr(void) {
	return cc_parse_comparison();
}

static cc_node_t *cc_parse_stmt(void) {
	if (g_cc_error) {
		return 0;
	}
	int line = g_cur.line;

	if (g_cur.type == TOK_LBRACE) {
		return cc_parse_block();
	}

	if (g_cur.type == TOK_KW_INT) {
		cc_advance();
		if (g_cur.type != TOK_IDENT) {
			cc_error(line, "esperado nome de variavel");
			return 0;
		}
		char name[32];
		cc_copy_text(name, g_cur.text);
		cc_advance();
		int offset = cc_sym_declare(name, line);
		if (g_cc_error) return 0;
		cc_node_t *node = cc_arena_alloc(sizeof(cc_node_t));
		if (!node) return 0;
		node->kind = ND_DECL;
		node->line = line;
		node->var_offset = offset;
		node->a = 0;
		node->next = 0;
		if (g_cur.type == TOK_ASSIGN) {
			cc_advance();
			node->a = cc_parse_expr();
		}
		cc_expect(TOK_SEMI, "esperado ';'");
		if (g_cc_error) return 0;
		return node;
	}

	if (g_cur.type == TOK_KW_IF) {
		cc_advance();
		cc_expect(TOK_LPAREN, "esperado '('");
		cc_node_t *cond = cc_parse_expr();
		cc_expect(TOK_RPAREN, "esperado ')'");
		cc_node_t *then_s = cc_parse_stmt();
		cc_node_t *else_s = 0;
		if (!g_cc_error && g_cur.type == TOK_KW_ELSE) {
			cc_advance();
			else_s = cc_parse_stmt();
		}
		if (g_cc_error) return 0;
		cc_node_t *node = cc_arena_alloc(sizeof(cc_node_t));
		if (!node) return 0;
		node->kind = ND_IF;
		node->line = line;
		node->a = cond;
		node->b = then_s;
		node->c = else_s;
		node->next = 0;
		return node;
	}

	if (g_cur.type == TOK_KW_WHILE) {
		cc_advance();
		cc_expect(TOK_LPAREN, "esperado '('");
		cc_node_t *cond = cc_parse_expr();
		cc_expect(TOK_RPAREN, "esperado ')'");
		cc_node_t *body = cc_parse_stmt();
		if (g_cc_error) return 0;
		cc_node_t *node = cc_arena_alloc(sizeof(cc_node_t));
		if (!node) return 0;
		node->kind = ND_WHILE;
		node->line = line;
		node->a = cond;
		node->b = body;
		node->next = 0;
		return node;
	}

	if (g_cur.type == TOK_KW_PRINT) {
		// Reserved-keyword statement, NOT a real function call -- there
		// is no call/argument-list grammar in this milestone at all.
		cc_advance();
		cc_expect(TOK_LPAREN, "esperado '('");
		cc_node_t *expr = cc_parse_expr();
		cc_expect(TOK_RPAREN, "esperado ')'");
		cc_expect(TOK_SEMI, "esperado ';'");
		if (g_cc_error) return 0;
		cc_node_t *node = cc_arena_alloc(sizeof(cc_node_t));
		if (!node) return 0;
		node->kind = ND_PRINT;
		node->line = line;
		node->a = expr;
		node->next = 0;
		return node;
	}

	if (g_cur.type == TOK_KW_RETURN) {
		cc_advance();
		cc_node_t *expr = cc_parse_expr();
		cc_expect(TOK_SEMI, "esperado ';'");
		if (g_cc_error) return 0;
		cc_node_t *node = cc_arena_alloc(sizeof(cc_node_t));
		if (!node) return 0;
		node->kind = ND_RETURN;
		node->line = line;
		node->a = expr;
		node->next = 0;
		return node;
	}

	if (g_cur.type == TOK_IDENT) {
		char name[32];
		cc_copy_text(name, g_cur.text);
		cc_advance();
		int offset = cc_sym_lookup(name);
		if (offset < 0) {
			cc_error(line, "variavel nao declarada");
			return 0;
		}
		cc_expect(TOK_ASSIGN, "esperado '='");
		cc_node_t *expr = cc_parse_expr();
		cc_expect(TOK_SEMI, "esperado ';'");
		if (g_cc_error) return 0;
		cc_node_t *node = cc_arena_alloc(sizeof(cc_node_t));
		if (!node) return 0;
		node->kind = ND_ASSIGN;
		node->line = line;
		node->var_offset = offset;
		node->a = expr;
		node->next = 0;
		return node;
	}

	cc_error(line, "comando invalido");
	return 0;
}

static cc_node_t *cc_parse_block(void) {
	cc_expect(TOK_LBRACE, "esperado '{'");
	if (g_cc_error) {
		return 0;
	}
	cc_node_t *block = cc_arena_alloc(sizeof(cc_node_t));
	if (!block) return 0;
	block->kind = ND_BLOCK;
	block->line = g_cur.line;
	block->a = 0;
	block->next = 0;
	cc_node_t *tail = 0;

	while (!g_cc_error && g_cur.type != TOK_RBRACE && g_cur.type != TOK_EOF) {
		cc_node_t *stmt = cc_parse_stmt();
		if (g_cc_error) {
			break;
		}
		if (!block->a) {
			block->a = stmt;
		} else {
			tail->next = stmt;
		}
		tail = stmt;
	}

	cc_expect(TOK_RBRACE, "esperado '}'");
	if (g_cc_error) {
		return 0;
	}
	return block;
}

static cc_node_t *cc_parse_program(void) {
	cc_expect(TOK_KW_INT, "esperado 'int'");
	if (g_cc_error) {
		return 0;
	}
	if (g_cur.type != TOK_IDENT || strcmp(g_cur.text, "main") != 0) {
		cc_error(g_cur.line, "esperado 'main'");
		return 0;
	}
	cc_advance();
	cc_expect(TOK_LPAREN, "esperado '('");
	cc_expect(TOK_RPAREN, "esperado ')'");
	if (g_cc_error) {
		return 0;
	}
	return cc_parse_block();
}

// ---- codegen: x86-32 stack machine, hand-encoded opcodes ----
// Every byte below is verified against the Intel encoding tables -- a
// wrong byte here crashes the whole kernel with zero memory protection,
// so these are not to be "simplified" without re-deriving the ModRM
// encoding by hand.
static unsigned char g_cc_code[CC_CODE_SIZE];
static int g_cc_code_pos;
static void (*g_cc_print_fn)(int);

static void emit_byte(unsigned char b) {
	if (g_cc_error) {
		return;
	}
	if (g_cc_code_pos >= CC_CODE_SIZE) {
		cc_error(-1, "codigo gerado excede o buffer");
		return;
	}
	g_cc_code[g_cc_code_pos++] = b;
}

static void emit_u32(unsigned int v) {
	emit_byte((unsigned char)(v & 0xFF));
	emit_byte((unsigned char)((v >> 8) & 0xFF));
	emit_byte((unsigned char)((v >> 16) & 0xFF));
	emit_byte((unsigned char)((v >> 24) & 0xFF));
}

static void emit_push_eax(void)      { emit_byte(0x50); }
static void emit_pop_eax(void)       { emit_byte(0x58); }
static void emit_mov_ecx_eax(void)   { emit_byte(0x89); emit_byte(0xC1); }
static void emit_mov_eax_imm32(unsigned int v) { emit_byte(0xB8); emit_u32(v); }
static void emit_add(void)           { emit_byte(0x01); emit_byte(0xC8); }
static void emit_sub(void)           { emit_byte(0x29); emit_byte(0xC8); }
static void emit_imul(void)          { emit_byte(0x0F); emit_byte(0xAF); emit_byte(0xC1); }
static void emit_cdq(void)           { emit_byte(0x99); }
static void emit_idiv_ecx(void)      { emit_byte(0xF7); emit_byte(0xF9); }
static void emit_neg_eax(void)       { emit_byte(0xF7); emit_byte(0xD8); }
static void emit_cmp(void)           { emit_byte(0x39); emit_byte(0xC8); }
static void emit_setcc_al(unsigned char cc) { emit_byte(0x0F); emit_byte(cc); emit_byte(0xC0); }
static void emit_movzx_eax_al(void)  { emit_byte(0x0F); emit_byte(0xB6); emit_byte(0xC0); }
static void emit_test_eax_eax(void)  { emit_byte(0x85); emit_byte(0xC0); }
static void emit_call_eax(void)      { emit_byte(0xFF); emit_byte(0xD0); }
static void emit_add_esp4(void)      { emit_byte(0x83); emit_byte(0xC4); emit_byte(0x04); }

static void emit_store_local(int off) { emit_byte(0x89); emit_byte(0x45); emit_byte((unsigned char)(-off)); }
static void emit_load_local(int off)  { emit_byte(0x8B); emit_byte(0x45); emit_byte((unsigned char)(-off)); }
static void emit_zero_local(int off)  { emit_byte(0xC7); emit_byte(0x45); emit_byte((unsigned char)(-off)); emit_u32(0); }

static void emit_prologue(int n) {
	emit_byte(0x55);             // push ebp
	emit_byte(0x89); emit_byte(0xE5); // mov ebp,esp
	if (n > 0) {
		emit_byte(0x81); emit_byte(0xEC); emit_u32((unsigned int)(4 * n)); // sub esp,4n
	}
}

static void emit_epilogue(void) {
	emit_byte(0x89); emit_byte(0xEC); // mov esp,ebp
	emit_byte(0x5D);                    // pop ebp
	emit_byte(0xC3);                    // ret
}

static int emit_jz_placeholder(void) {
	emit_test_eax_eax();
	emit_byte(0x0F);
	emit_byte(0x84);
	int at = g_cc_code_pos;
	emit_u32(0);
	return at;
}

static int emit_jmp_placeholder(void) {
	emit_byte(0xE9);
	int at = g_cc_code_pos;
	emit_u32(0);
	return at;
}

static void emit_jmp_back(int target) {
	emit_byte(0xE9);
	emit_u32((unsigned int)(target - (g_cc_code_pos + 4)));
}

static void cc_patch(int at, int target) {
	if (g_cc_error) {
		return;
	}
	unsigned int rel = (unsigned int)(target - (at + 4));
	g_cc_code[at]     = (unsigned char)(rel & 0xFF);
	g_cc_code[at + 1] = (unsigned char)((rel >> 8) & 0xFF);
	g_cc_code[at + 2] = (unsigned char)((rel >> 16) & 0xFF);
	g_cc_code[at + 3] = (unsigned char)((rel >> 24) & 0xFF);
}

static void cc_codegen_expr(cc_node_t *n) {
	if (g_cc_error || !n) {
		return;
	}
	switch (n->kind) {
		case ND_NUM:
			emit_mov_eax_imm32((unsigned int)n->int_val);
			break;
		case ND_VAR:
			emit_load_local(n->var_offset);
			break;
		case ND_UNM:
			cc_codegen_expr(n->a);
			emit_neg_eax();
			break;
		case ND_BINOP:
			cc_codegen_expr(n->a);   // -> eax (left)
			emit_push_eax();
			cc_codegen_expr(n->b);   // -> eax (right)
			emit_mov_ecx_eax();       // ecx = right
			emit_pop_eax();           // eax = left
			switch (n->op) {
				case TOK_PLUS:  emit_add(); break;
				case TOK_MINUS: emit_sub(); break;
				case TOK_STAR:  emit_imul(); break;
				case TOK_SLASH: emit_cdq(); emit_idiv_ecx(); break;
				case TOK_EQ: emit_cmp(); emit_setcc_al(0x94); emit_movzx_eax_al(); break;
				case TOK_NE: emit_cmp(); emit_setcc_al(0x95); emit_movzx_eax_al(); break;
				case TOK_LT: emit_cmp(); emit_setcc_al(0x9C); emit_movzx_eax_al(); break;
				case TOK_GT: emit_cmp(); emit_setcc_al(0x9F); emit_movzx_eax_al(); break;
				case TOK_LE: emit_cmp(); emit_setcc_al(0x9E); emit_movzx_eax_al(); break;
				case TOK_GE: emit_cmp(); emit_setcc_al(0x9D); emit_movzx_eax_al(); break;
				default: cc_error(n->line, "operador invalido"); break;
			}
			break;
		default:
			cc_error(n->line, "expressao invalida");
			break;
	}
}

static void cc_codegen_stmt(cc_node_t *n) {
	if (g_cc_error || !n) {
		return;
	}
	switch (n->kind) {
		case ND_BLOCK: {
			cc_node_t *s = n->a;
			while (s && !g_cc_error) {
				cc_codegen_stmt(s);
				s = s->next;
			}
			break;
		}
		case ND_DECL:
			if (n->a) {
				cc_codegen_expr(n->a);
				emit_store_local(n->var_offset);
			} else {
				emit_zero_local(n->var_offset); // no uninitialized-stack
				                                  // garbage on read, see cc.h
			}
			break;
		case ND_ASSIGN:
			cc_codegen_expr(n->a);
			emit_store_local(n->var_offset);
			break;
		case ND_IF: {
			cc_codegen_expr(n->a);
			int l1 = emit_jz_placeholder();
			cc_codegen_stmt(n->b);
			if (n->c) {
				int l2 = emit_jmp_placeholder();
				cc_patch(l1, g_cc_code_pos);
				cc_codegen_stmt(n->c);
				cc_patch(l2, g_cc_code_pos);
			} else {
				cc_patch(l1, g_cc_code_pos);
			}
			break;
		}
		case ND_WHILE: {
			int start = g_cc_code_pos;
			cc_codegen_expr(n->a);
			int l1 = emit_jz_placeholder();
			cc_codegen_stmt(n->b);
			emit_jmp_back(start);
			cc_patch(l1, g_cc_code_pos);
			break;
		}
		case ND_PRINT:
			cc_codegen_expr(n->a);
			emit_push_eax();                                    // sole cdecl arg
			emit_mov_eax_imm32((unsigned int)g_cc_print_fn);
			emit_call_eax();
			emit_add_esp4();
			break;
		case ND_RETURN:
			cc_codegen_expr(n->a);
			emit_epilogue();
			break;
		default:
			cc_error(n->line, "comando invalido");
			break;
	}
}

// g_cc_code is reused across cc_compile() calls -- overwriting a buffer
// that was previously fetched/executed by the CPU (or cached as a
// translated block by an emulator's JIT) without a serializing
// instruction in between is a classic self-modifying-code hazard: the
// processor is architecturally permitted to keep executing stale decoded
// bytes from before the overwrite for some window after the write.
// CPUID is a universally available serializing instruction; run it once
// right after codegen finishes and before the caller is handed the
// buffer to call.
static void cc_serialize(void) {
	unsigned int a = 0, b, c, d;
	__asm__ volatile ("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
	(void)b; (void)c; (void)d;
}

// ---- driver ----
int cc_compile(const char *src, int src_len, void (*print_fn)(int),
                unsigned char **out_code,
                char *err_msg, int err_msg_max, int *err_line) {
	g_cc_arena_pos = 0;
	g_cc_code_pos = 0;
	g_cc_sym_count = 0;
	g_cc_error = 0;
	g_cc_err_msg[0] = 0;
	g_cc_err_line = -1;
	g_cc_print_fn = print_fn;

	g_src = src;
	g_src_len = src_len;
	g_pos = 0;
	g_line = 1;
	cc_advance(); // prime g_cur with the first token

	cc_node_t *program = cc_parse_program();

	if (!g_cc_error) {
		emit_prologue(g_cc_sym_count);
		cc_codegen_stmt(program);
		// Mandatory fall-off-the-end safety net: if main()'s body never
		// hits a `return` at runtime, execution must not continue past
		// the generated code into whatever leftover/uninitialized bytes
		// sit after it in g_cc_code.
		emit_mov_eax_imm32(0);
		emit_epilogue();
	}

	if (g_cc_error) {
		if (out_code) {
			*out_code = 0;
		}
		if (err_line) {
			*err_line = g_cc_err_line;
		}
		if (err_msg && err_msg_max > 0) {
			int i = 0;
			while (g_cc_err_msg[i] && i < err_msg_max - 1) {
				err_msg[i] = g_cc_err_msg[i];
				i++;
			}
			err_msg[i] = 0;
		}
		return -1;
	}

	cc_serialize();

	if (out_code) {
		*out_code = g_cc_code;
	}
	return g_cc_code_pos;
}

int cc_save_program(unsigned char *out_buf, int out_buf_max,
                     const unsigned char *code, int code_len) {
	if (code_len < 0 || CC_PROGRAM_HEADER_SIZE + code_len > out_buf_max) {
		return -1;
	}
	out_buf[0] = 'R';
	out_buf[1] = 'K';
	out_buf[2] = 'X';
	out_buf[3] = 'C';
	out_buf[4] = (unsigned char)(code_len & 0xFF);
	out_buf[5] = (unsigned char)((code_len >> 8) & 0xFF);
	out_buf[6] = (unsigned char)((code_len >> 16) & 0xFF);
	out_buf[7] = (unsigned char)((code_len >> 24) & 0xFF);
	memcpy(out_buf + CC_PROGRAM_HEADER_SIZE, code, code_len);
	return CC_PROGRAM_HEADER_SIZE + code_len;
}

int cc_load_program(const unsigned char *file_bytes, int file_len,
                     unsigned char **out_code) {
	if (out_code) {
		*out_code = 0;
	}
	if (file_len < CC_PROGRAM_HEADER_SIZE) {
		return -1;
	}
	if (file_bytes[0] != 'R' || file_bytes[1] != 'K' || file_bytes[2] != 'X' || file_bytes[3] != 'C') {
		return -1;
	}
	int code_len = (int)file_bytes[4] | ((int)file_bytes[5] << 8) |
	               ((int)file_bytes[6] << 16) | ((int)file_bytes[7] << 24);
	if (code_len < 0 || code_len > CC_CODE_SIZE || CC_PROGRAM_HEADER_SIZE + code_len != file_len) {
		return -1;
	}

	memcpy(g_cc_code, file_bytes + CC_PROGRAM_HEADER_SIZE, code_len);
	cc_serialize(); // same reused-executable-buffer hazard cc_compile() has

	if (out_code) {
		*out_code = g_cc_code;
	}
	return code_len;
}
