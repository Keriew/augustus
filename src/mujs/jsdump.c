#include "jsi.h"
#include "jsparse.h"
#include "jscompile.h"
#include "jsvalue.h"

#include "utf.h"

#include <assert.h>

static const char *astname[] = {
#include "astnames.h"
};

static const char *opname[] = {
#include "opnames.h"
};

const char *jsP_aststring(enum js_AstType type)
{
	if (type < nelem(astname))
		return astname[type];
	return "<unknown>";
}

const char *jsC_opcodestring(enum js_OpCode opcode)
{
	if (opcode < nelem(opname))
		return opname[opcode];
	return "<unknown>";
}

static int prec(enum js_AstType type)
{
	switch (type) {
	case EXP_IDENTIFIER:
	case EXP_NUMBER:
	case EXP_STRING:
	case EXP_REGEXP:
	case EXP_UNDEF:
	case EXP_NULL:
	case EXP_TRUE:
	case EXP_FALSE:
	case EXP_THIS:
	case EXP_ARRAY:
	case EXP_OBJECT:
		return 170;

	case EXP_FUN:
	case EXP_INDEX:
	case EXP_MEMBER:
	case EXP_CALL:
	case EXP_NEW:
		return 160;

	case EXP_POSTINC:
	case EXP_POSTDEC:
		return 150;

	case EXP_DELETE:
	case EXP_VOID:
	case EXP_TYPEOF:
	case EXP_PREINC:
	case EXP_PREDEC:
	case EXP_POS:
	case EXP_NEG:
	case EXP_BITNOT:
	case EXP_LOGNOT:
		return 140;

	case EXP_MOD:
	case EXP_DIV:
	case EXP_MUL:
		return 130;

	case EXP_SUB:
	case EXP_ADD:
		return 120;

	case EXP_USHR:
	case EXP_SHR:
	case EXP_SHL:
		return 110;

	case EXP_IN:
	case EXP_INSTANCEOF:
	case EXP_GE:
	case EXP_LE:
	case EXP_GT:
	case EXP_LT:
		return 100;

	case EXP_STRICTNE:
	case EXP_STRICTEQ:
	case EXP_NE:
	case EXP_EQ:
		return 90;

	case EXP_BITAND: return 80;
	case EXP_BITXOR: return 70;
	case EXP_BITOR: return 60;
	case EXP_LOGAND: return 50;
	case EXP_LOGOR: return 40;

	case EXP_COND:
		return 30;

	case EXP_ASS:
	case EXP_ASS_MUL:
	case EXP_ASS_DIV:
	case EXP_ASS_MOD:
	case EXP_ASS_ADD:
	case EXP_ASS_SUB:
	case EXP_ASS_SHL:
	case EXP_ASS_SHR:
	case EXP_ASS_USHR:
	case EXP_ASS_BITAND:
	case EXP_ASS_BITXOR:
	case EXP_ASS_BITOR:
		return 20;

#define COMMA 15

	case EXP_COMMA:
		return 10;

	default:
		return 0;
	}
}

static void pc(int c)
{
	putchar(c);
}

static void ps(const char *s)
{
	fputs(s, stdout);
}

static void in(int d)
{
	while (d-- > 0)
		putchar('\t');
}

static void nl(void)
{
	putchar('\n');
}

/* Pretty-printed Javascript syntax */

static void pstmlist(int d, js_Ast *list);
static void pexpi(int d, int i, js_Ast *exp);
static void pstm(int d, js_Ast *stm);
static void slist(int d, js_Ast *list);
static void sblock(int d, js_Ast *list);

static void pargs(int d, js_Ast *list)
{
	while (list) {
		assert(list->type == AST_LIST);
		pexpi(d, COMMA, list->a);
		list = list->b;
		if (list)
			ps(", ");
	}
}

static void parray(int d, js_Ast *list)
{
	ps("[");
	while (list) {
		assert(list->type == AST_LIST);
		pexpi(d, COMMA, list->a);
		list = list->b;
		if (list)
			ps(", ");
	}
	ps("]");
}

static void pobject(int d, js_Ast *list)
{
	ps("{");
	while (list) {
		js_Ast *kv = list->a;
		assert(list->type == AST_LIST);
		switch (kv->type) {
		default: break;
		case EXP_PROP_VAL:
			pexpi(d, COMMA, kv->a);
			ps(": ");
			pexpi(d, COMMA, kv->b);
			break;
		case EXP_PROP_GET:
			ps("get ");
			pexpi(d, COMMA, kv->a);
			ps("() {\n");
			pstmlist(d, kv->c);
			in(d); ps("}");
			break;
		case EXP_PROP_SET:
			ps("set ");
			pexpi(d, COMMA, kv->a);
			ps("(");
			pargs(d, kv->b);
			ps(") {\n");
			pstmlist(d, kv->c);
			in(d); ps("}");
			break;
		}
		list = list->b;
		if (list)
			ps(", ");
	}
	ps("}");
}

static void pstr(const char *s)
{
	static const char *HEX = "0123456789ABCDEF";
	Rune c;
	pc('"');
	while (*s) {
		s += chartorune(&c, s);
		switch (c) {
		case '"': ps("\\\""); break;
		case '\\': ps("\\\\"); break;
		case '\b': ps("\\b"); break;
		case '\f': ps("\\f"); break;
		case '\n': ps("\\n"); break;
		case '\r': ps("\\r"); break;
		case '\t': ps("\\t"); break;
		default:
			if (c < ' ' || c > 127) {
				ps("\\u");
				pc(HEX[(c>>12)&15]);
				pc(HEX[(c>>8)&15]);
				pc(HEX[(c>>4)&15]);
				pc(HEX[c&15]);
			} else {
				pc(c); break;
			}
		}
	}
	pc('"');
}

static void pregexp(const char *prog, int flags)
{
	pc('/');
	ps(prog);
	pc('/');
	if (flags & JS_REGEXP_G) pc('g');
	if (flags & JS_REGEXP_I) pc('i');
	if (flags & JS_REGEXP_M) pc('m');
}

static void pbin(int d, int p, js_Ast *exp, const char *op)
{
	pexpi(d, p, exp->a);
	ps(op);
	pexpi(d, p, exp->b);
}

static void puna(int d, int p, js_Ast *exp, const char *pre, const char *suf)
{
	ps(pre);
	pexpi(d, p, exp->a);
	ps(suf);
}

static void pexpi(int d, int p, js_Ast *exp)
{
	int tp = prec(exp->type);
	int paren = 0;
	if (tp < p) {
		pc('(');
		paren = 1;
	}
	p = tp;

	switch (exp->type) {
	case AST_IDENTIFIER: ps(exp->string); break;
	case EXP_IDENTIFIER: ps(exp->string); break;
	case EXP_NUMBER: printf("%.9g", exp->number); break;
	case EXP_STRING: pstr(exp->string); break;
	case EXP_REGEXP: pregexp(exp->string, exp->number); break;

	case EXP_UNDEF: break;
	case EXP_NULL: ps("null"); break;
	case EXP_TRUE: ps("true"); break;
	case EXP_FALSE: ps("false"); break;
	case EXP_THIS: ps("this"); break;

	case EXP_OBJECT: pobject(d, exp->a); break;
	case EXP_ARRAY: parray(d, exp->a); break;

	case EXP_DELETE: puna(d, p, exp, "delete ", ""); break;
	case EXP_VOID: puna(d, p, exp, "void ", ""); break;
	case EXP_TYPEOF: puna(d, p, exp, "typeof ", ""); break;
	case EXP_PREINC: puna(d, p, exp, "++", ""); break;
	case EXP_PREDEC: puna(d, p, exp, "--", ""); break;
	case EXP_POSTINC: puna(d, p, exp, "", "++"); break;
	case EXP_POSTDEC: puna(d, p, exp, "", "--"); break;
	case EXP_POS: puna(d, p, exp, "+", ""); break;
	case EXP_NEG: puna(d, p, exp, "-", ""); break;
	case EXP_BITNOT: puna(d, p, exp, "~", ""); break;
	case EXP_LOGNOT: puna(d, p, exp, "!", ""); break;

	case EXP_LOGOR: pbin(d, p, exp, " || "); break;
	case EXP_LOGAND: pbin(d, p, exp, " && "); break;
	case EXP_BITOR: pbin(d, p, exp, " | "); break;
	case EXP_BITXOR: pbin(d, p, exp, " ^ "); break;
	case EXP_BITAND: pbin(d, p, exp, " & "); break;
	case EXP_EQ: pbin(d, p, exp, " == "); break;
	case EXP_NE: pbin(d, p, exp, " != "); break;
	case EXP_STRICTEQ: pbin(d, p, exp, " === "); break;
	case EXP_STRICTNE: pbin(d, p, exp, " !== "); break;
	case EXP_LT: pbin(d, p, exp, " < "); break;
	case EXP_GT: pbin(d, p, exp, " > "); break;
	case EXP_LE: pbin(d, p, exp, " <= "); break;
	case EXP_GE: pbin(d, p, exp, " >= "); break;
	case EXP_INSTANCEOF: pbin(d, p, exp, " instanceof "); break;
	case EXP_IN: pbin(d, p, exp, " in "); break;
	case EXP_SHL: pbin(d, p, exp, " << "); break;
	case EXP_SHR: pbin(d, p, exp, " >> "); break;
	case EXP_USHR: pbin(d, p, exp, " >>> "); break;
	case EXP_ADD: pbin(d, p, exp, " + "); break;
	case EXP_SUB: pbin(d, p, exp, " - "); break;
	case EXP_MUL: pbin(d, p, exp, " * "); break;
	case EXP_DIV: pbin(d, p, exp, " / "); break;
	case EXP_MOD: pbin(d, p, exp, " % "); break;
	case EXP_ASS: pbin(d, p, exp, " = "); break;
	case EXP_ASS_MUL: pbin(d, p, exp, " *= "); break;
	case EXP_ASS_DIV: pbin(d, p, exp, " /= "); break;
	case EXP_ASS_MOD: pbin(d, p, exp, " %= "); break;
	case EXP_ASS_ADD: pbin(d, p, exp, " += "); break;
	case EXP_ASS_SUB: pbin(d, p, exp, " -= "); break;
	case EXP_ASS_SHL: pbin(d, p, exp, " <<= "); break;
	case EXP_ASS_SHR: pbin(d, p, exp, " >>= "); break;
	case EXP_ASS_USHR: pbin(d, p, exp, " >>>= "); break;
	case EXP_ASS_BITAND: pbin(d, p, exp, " &= "); break;
	case EXP_ASS_BITXOR: pbin(d, p, exp, " ^= "); break;
	case EXP_ASS_BITOR: pbin(d, p, exp, " |= "); break;

	case EXP_COMMA: pbin(d, p, exp, ", "); break;

	case EXP_COND:
		pexpi(d, p, exp->a);
		ps(" ? ");
		pexpi(d, p, exp->b);
		ps(" : ");
		pexpi(d, p, exp->c);
		break;

	case EXP_INDEX:
		pexpi(d, p, exp->a);
		pc('[');
		pexpi(d, 0, exp->b);
		pc(']');
		break;

	case EXP_MEMBER:
		pexpi(d, p, exp->a);
		pc('.');
		pexpi(d, 0, exp->b);
		break;

	case EXP_CALL:
		pexpi(d, p, exp->a);
		pc('(');
		pargs(d, exp->b);
		pc(')');
		break;

	case EXP_NEW:
		ps("new ");
		pexpi(d, p, exp->a);
		pc('(');
		pargs(d, exp->b);
		pc(')');
		break;

	case EXP_FUN:
		if (p == 0) pc('(');
		ps("function ");
		if (exp->a) pexpi(d, 0, exp->a);
		pc('(');
		pargs(d, exp->b);
		ps(") {\n");
		pstmlist(d, exp->c);
		in(d); pc('}');
		if (p == 0) pc(')');
		break;

	default:
		ps("<UNKNOWN>");
		break;
	}

	if (paren) pc(')');
}

static void pexp(int d, js_Ast *exp)
{
	pexpi(d, 0, exp);
}

static void pvar(int d, js_Ast *var)
{
	assert(var->type == EXP_VAR);
	pexp(d, var->a);
	if (var->b) {
		ps(" = ");
		pexp(d, var->b);
	}
}

static void pvarlist(int d, js_Ast *list)
{
	while (list) {
		assert(list->type == AST_LIST);
		pvar(d, list->a);
		list = list->b;
		if (list)
			ps(", ");
	}
}

static void pblock(int d, js_Ast *block)
{
	assert(block->type == STM_BLOCK);
	ps(" {\n");
	pstmlist(d, block->a);
	in(d); pc('}');
}

static void pstmh(int d, js_Ast *stm)
{
	if (stm->type == STM_BLOCK)
		pblock(d, stm);
	else {
		nl();
		pstm(d+1, stm);
	}
}

static void pcaselist(int d, js_Ast *list)
{
	while (list) {
		js_Ast *stm = list->a;
		if (stm->type == STM_CASE) {
			in(d); ps("case "); pexp(d, stm->a); ps(":\n");
			pstmlist(d, stm->b);
		}
		if (stm->type == STM_DEFAULT) {
			in(d); ps("default:\n");
			pstmlist(d, stm->a);
		}
		list = list->b;
	}
}

static void pstm(int d, js_Ast *stm)
{
	if (stm->type == STM_BLOCK) {
		pblock(d, stm);
		return;
	}

	in(d);

	switch (stm->type) {
	case AST_FUNDEC:
		ps("function ");
		pexp(d, stm->a);
		pc('(');
		pargs(d, stm->b);
		ps(") {\n");
		pstmlist(d, stm->c);
		in(d); ps("}");
		break;

	case STM_EMPTY:
		pc(';');
		break;

	case STM_VAR:
		ps("var ");
		pvarlist(d, stm->a);
		ps(";");
		break;

	case STM_IF:
		ps("if ("); pexp(d, stm->a); ps(")");
		pstmh(d, stm->b);
		if (stm->c) {
			nl(); in(d); ps("else");
			pstmh(d, stm->c);
		}
		break;

	case STM_DO:
		ps("do");
		pstmh(d, stm->a);
		nl();
		in(d); ps("while ("); pexp(d, stm->b); ps(");");
		break;

	case STM_WHILE:
		ps("while ("); pexp(d, stm->a); ps(")");
		pstmh(d, stm->b);
		break;

	case STM_FOR:
		ps("for (");
		pexp(d, stm->a); ps("; ");
		pexp(d, stm->b); ps("; ");
		pexp(d, stm->c); ps(")");
		pstmh(d, stm->d);
		break;
	case STM_FOR_VAR:
		ps("for (var ");
		pvarlist(d, stm->a); ps("; ");
		pexp(d, stm->b); ps("; ");
		pexp(d, stm->c); ps(")");
		pstmh(d, stm->d);
		break;
	case STM_FOR_IN:
		ps("for (");
		pexp(d, stm->a); ps(" in ");
		pexp(d, stm->b); ps(")");
		pstmh(d, stm->c);
		break;
	case STM_FOR_IN_VAR:
		ps("for (var ");
		pvarlist(d, stm->a); ps(" in ");
		pexp(d, stm->b); ps(")");
		pstmh(d, stm->c);
		break;

	case STM_CONTINUE:
		if (stm->a) {
			ps("continue "); pexp(d, stm->a); ps(";");
		} else {
			ps("continue;");
		}
		break;

	case STM_BREAK:
		if (stm->a) {
			ps("break "); pexp(d, stm->a); ps(";");
		} else {
			ps("break;");
		}
		break;

	case STM_RETURN:
		if (stm->a) {
			ps("return "); pexp(d, stm->a); ps(";");
		} else {
			ps("return;");
		}
		break;

	case STM_WITH:
		ps("with ("); pexp(d, stm->a); ps(")");
		pstmh(d, stm->b);
		break;

	case STM_SWITCH:
		ps("switch (");
		pexp(d, stm->a);
		ps(") {\n");
		pcaselist(d, stm->b);
		in(d); ps("}");
		break;

	case STM_THROW:
		ps("throw "); pexp(d, stm->a); ps(";");
		break;

	case STM_TRY:
		ps("try");
		pstmh(d, stm->a);
		if (stm->b && stm->c) {
			nl(); in(d); ps("catch ("); pexp(d, stm->b); ps(")");
			pstmh(d, stm->c);
		}
		if (stm->d) {
			nl(); in(d); ps("finally");
			pstmh(d, stm->d);
		}
		break;

	case STM_LABEL:
		pexp(d, stm->a); ps(": "); pstm(d, stm->b);
		break;

	case STM_DEBUGGER:
		ps("debugger;");
		break;

	default:
		pexp(d, stm); pc(';');
	}
}

static void pstmlist(int d, js_Ast *list)
{
	while (list) {
		assert(list->type == AST_LIST);
		pstm(d+1, list->a);
		nl();
		list = list->b;
	}
}

void jsP_dumpsyntax(js_State *J, js_Ast *prog)
{
	if (prog->type == AST_LIST)
		pstmlist(-1, prog);
	else {
		pstm(0, prog);
		nl();
	}
}

/* S-expression list representation */

static void snode(int d, js_Ast *node)
{
	void (*afun)(int,js_Ast*) = snode;
	void (*bfun)(int,js_Ast*) = snode;
	void (*cfun)(int,js_Ast*) = snode;
	void (*dfun)(int,js_Ast*) = snode;

	if (!node) {
		return;
	}

	if (node->type == AST_LIST) {
		slist(d, node);
		return;
	}

	pc('(');
	ps(astname[node->type]);
	switch (node->type) {
	default: break;
	case AST_IDENTIFIER: pc(' '); ps(node->string); break;
	case EXP_IDENTIFIER: pc(' '); ps(node->string); break;
	case EXP_STRING: pc(' '); pstr(node->string); break;
	case EXP_REGEXP: pc(' '); pregexp(node->string, node->number); break;
	case EXP_NUMBER: printf(" %.9g", node->number); break;
	case STM_BLOCK: afun = sblock; break;
	case AST_FUNDEC: case EXP_FUN: cfun = sblock; break;
	case EXP_PROP_GET: cfun = sblock; break;
	case EXP_PROP_SET: cfun = sblock; break;
	case STM_SWITCH: bfun = sblock; break;
	case STM_CASE: bfun = sblock; break;
	case STM_DEFAULT: afun = sblock; break;
	}
	if (node->a) { pc(' '); afun(d, node->a); }
	if (node->b) { pc(' '); bfun(d, node->b); }
	if (node->c) { pc(' '); cfun(d, node->c); }
	if (node->d) { pc(' '); dfun(d, node->d); }
	pc(')');
}

static void slist(int d, js_Ast *list)
{
	pc('[');
	while (list) {
		assert(list->type == AST_LIST);
		snode(d, list->a);
		list = list->b;
		if (list)
			pc(' ');
	}
	pc(']');
}

static void sblock(int d, js_Ast *list)
{
	ps("[\n");
	in(d+1);
	while (list) {
		assert(list->type == AST_LIST);
		snode(d+1, list->a);
		list = list->b;
		if (list) {
			nl();
			in(d+1);
		}
	}
	nl(); in(d); pc(']');
}

void jsP_dumplist(js_State *J, js_Ast *prog)
{
	if (prog->type == AST_LIST)
		sblock(0, prog);
	else
		snode(0, prog);
	nl();
}

/* Compiled code */

void jsC_dumpfunction(js_State *J, js_Function *F)
{
	js_Instruction *p = F->code;
	js_Instruction *end = F->code + F->codelen;
	int i;

	printf("%s(%d)\n", F->name, F->numparams);
	if (F->lightweight) printf("\tlightweight\n");
	if (F->arguments) printf("\targuments\n");
	printf("\tsource %s:%d\n", F->filename, F->line);
	for (i = 0; i < F->funlen; ++i)
		printf("\tfunction %d %s\n", i, F->funtab[i]->name);
	for (i = 0; i < F->varlen; ++i)
		printf("\tlocal %d %s\n", i + 1, F->vartab[i]);

	printf("{\n");
	while (p < end) {
		int c = *p++;

		printf("% 5d: ", (int)(p - F->code) - 1);
		ps(opname[c]);

		switch (c) {
		case OP_NUMBER:
			printf(" %.9g", F->numtab[*p++]);
			break;
		case OP_STRING:
			pc(' ');
			pstr(F->strtab[*p++]);
			break;
		case OP_NEWREGEXP:
			pc(' ');
			pregexp(F->strtab[p[0]], p[1]);
			p += 2;
			break;

		case OP_INITVAR:
		case OP_DEFVAR:
		case OP_GETVAR:
		case OP_SETVAR:
		case OP_DELVAR:
		case OP_GETPROP_S:
		case OP_SETPROP_S:
		case OP_DELPROP_S:
		case OP_CATCH:
			pc(' ');
			ps(F->strtab[*p++]);
			break;

		case OP_LINE:
		case OP_CLOSURE:
		case OP_INITLOCAL:
		case OP_GETLOCAL:
		case OP_SETLOCAL:
		case OP_DELLOCAL:
		case OP_NUMBER_POS:
		case OP_NUMBER_NEG:
		case OP_CALL:
		case OP_NEW:
		case OP_JUMP:
		case OP_JTRUE:
		case OP_JFALSE:
		case OP_JCASE:
		case OP_TRY:
			printf(" %d", *p++);
			break;
		}

		nl();
	}
	printf("}\n");

	for (i = 0; i < F->funlen; ++i) {
		if (F->funtab[i] != F) {
			printf("function %d ", i);
			jsC_dumpfunction(J, F->funtab[i]);
		}
	}
}

/* Runtime values */

void js_dumpvalue(js_State *J, js_Value v)
{
	switch (v.type) {
	case JS_TUNDEFINED: printf("undefined"); break;
	case JS_TNULL: printf("null"); break;
	case JS_TBOOLEAN: printf(v.u.boolean ? "true" : "false"); break;
	case JS_TNUMBER: printf("%.9g", v.u.number); break;
	case JS_TSHRSTR: printf("'%s'", v.u.shrstr); break;
	case JS_TLITSTR: printf("'%s'", v.u.litstr); break;
	case JS_TMEMSTR: printf("'%s'", v.u.memstr->p); break;
	case JS_TOBJECT:
		if (v.u.object == J->G) {
			printf("[Global]");
			break;
		}
		switch (v.u.object->type) {
		case JS_COBJECT: printf("[Object %p]", v.u.object); break;
		case JS_CARRAY: printf("[Array %p]", v.u.object); break;
		case JS_CFUNCTION:
			printf("[Function %p, %s, %s:%d]",
				v.u.object,
				v.u.object->u.f.function->name,
				v.u.object->u.f.function->filename,
				v.u.object->u.f.function->line);
			break;
		case JS_CSCRIPT: printf("[Script %s]", v.u.object->u.f.function->filename); break;
		case JS_CCFUNCTION: printf("[CFunction %p]", v.u.object->u.c.function); break;
		case JS_CBOOLEAN: printf("[Boolean %d]", v.u.object->u.boolean); break;
		case JS_CNUMBER: printf("[Number %g]", v.u.object->u.number); break;
		case JS_CSTRING: printf("[String'%s']", v.u.object->u.s.string); break;
		case JS_CERROR: printf("[Error %s]", v.u.object->u.s.string); break;
		case JS_CITERATOR: printf("[Iterator %p]", v.u.object); break;
		case JS_CUSERDATA:
			printf("[Userdata %s %p]", v.u.object->u.user.tag, v.u.object->u.user.data);
			break;
		default: printf("[Object %p]", v.u.object); break;
		}
		break;
	}
}

static void js_dumpproperty(js_State *J, js_Property *node)
{
	if (node->left->level)
		js_dumpproperty(J, node->left);
	printf("\t%s: ", node->name);
	js_dumpvalue(J, node->value);
	printf(",\n");
	if (node->right->level)
		js_dumpproperty(J, node->right);
}

void js_dumpobject(js_State *J, js_Object *obj)
{
	printf("{\n");
	if (obj->properties->level)
		js_dumpproperty(J, obj->properties);
	printf("}\n");
}
