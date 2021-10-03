#include "jsi.h"
#include "jslex.h"
#include "jsvalue.h"
#include "jsbuiltin.h"

#include "utf.h"

static void jsonnext(js_State *J)
{
	J->lookahead = jsY_lexjson(J);
}

static int jsonaccept(js_State *J, int t)
{
	if (J->lookahead == t) {
		jsonnext(J);
		return 1;
	}
	return 0;
}

static void jsonexpect(js_State *J, int t)
{
	if (!jsonaccept(J, t))
		js_syntaxerror(J, "JSON: unexpected token: %s (expected %s)",
				jsY_tokenstring(J->lookahead), jsY_tokenstring(t));
}

static void jsonvalue(js_State *J)
{
	int i;
	const char *name;

	switch (J->lookahead) {
	case TK_STRING:
		js_pushliteral(J, J->text);
		jsonnext(J);
		break;

	case TK_NUMBER:
		js_pushnumber(J, J->number);
		jsonnext(J);
		break;

	case '{':
		js_newobject(J);
		jsonnext(J);
		if (jsonaccept(J, '}'))
			return;
		do {
			if (J->lookahead != TK_STRING)
				js_syntaxerror(J, "JSON: unexpected token: %s (expected string)", jsY_tokenstring(J->lookahead));
			name = J->text;
			jsonnext(J);
			jsonexpect(J, ':');
			jsonvalue(J);
			js_setproperty(J, -2, name);
		} while (jsonaccept(J, ','));
		jsonexpect(J, '}');
		break;

	case '[':
		js_newarray(J);
		jsonnext(J);
		i = 0;
		if (jsonaccept(J, ']'))
			return;
		do {
			jsonvalue(J);
			js_setindex(J, -2, i++);
		} while (jsonaccept(J, ','));
		jsonexpect(J, ']');
		break;

	case TK_TRUE:
		js_pushboolean(J, 1);
		jsonnext(J);
		break;

	case TK_FALSE:
		js_pushboolean(J, 0);
		jsonnext(J);
		break;

	case TK_NULL:
		js_pushnull(J);
		jsonnext(J);
		break;

	default:
		js_syntaxerror(J, "JSON: unexpected token: %s", jsY_tokenstring(J->lookahead));
	}
}

static void JSON_parse(js_State *J)
{
	const char *source = js_tostring(J, 1);
	jsY_initlex(J, "JSON", source);
	jsonnext(J);
	jsonvalue(J);
	// TODO: reviver Walk()
}

static void fmtnum(js_State *J, js_Buffer **sb, double n)
{
	if (isnan(n)) js_puts(J, sb, "null");
	else if (isinf(n)) js_puts(J, sb, "null");
	else if (n == 0) js_puts(J, sb, "0");
	else {
		char buf[40];
		sprintf(buf, "%.17g", n);
		js_puts(J, sb, buf);
	}
}

static void fmtstr(js_State *J, js_Buffer **sb, const char *s)
{
	static const char *HEX = "0123456789ABCDEF";
	Rune c;
	js_putc(J, sb, '"');
	while (*s) {
		s += chartorune(&c, s);
		switch (c) {
		case '"': js_puts(J, sb, "\\\""); break;
		case '\\': js_puts(J, sb, "\\\\"); break;
		case '\b': js_puts(J, sb, "\\b"); break;
		case '\f': js_puts(J, sb, "\\f"); break;
		case '\n': js_puts(J, sb, "\\n"); break;
		case '\r': js_puts(J, sb, "\\r"); break;
		case '\t': js_puts(J, sb, "\\t"); break;
		default:
			if (c < ' ') {
				js_puts(J, sb, "\\u");
				js_putc(J, sb, HEX[(c>>12)&15]);
				js_putc(J, sb, HEX[(c>>8)&15]);
				js_putc(J, sb, HEX[(c>>4)&15]);
				js_putc(J, sb, HEX[c&15]);
			} else {
				js_putc(J, sb, c); break;
			}
		}
	}
	js_putc(J, sb, '"');
}

static void fmtindent(js_State *J, js_Buffer **sb, const char *gap, int level)
{
	js_putc(J, sb, '\n');
	while (level--)
		js_puts(J, sb, gap);
}

static int fmtvalue(js_State *J, js_Buffer **sb, const char *key, const char *gap, int level);

static void fmtobject(js_State *J, js_Buffer **sb, js_Object *obj, const char *gap, int level)
{
	js_Property *ref;
	int save;
	int n = 0;

	js_putc(J, sb, '{');
	for (ref = obj->head; ref; ref = ref->next) {
		if (ref->atts & JS_DONTENUM)
			continue;
		save = (*sb)->n;
		if (n) js_putc(J, sb, ',');
		if (gap) fmtindent(J, sb, gap, level + 1);
		fmtstr(J, sb, ref->name);
		js_putc(J, sb, ':');
		if (gap)
			js_putc(J, sb, ' ');
		js_pushvalue(J, ref->value);
		if (!fmtvalue(J, sb, ref->name, gap, level + 1))
			(*sb)->n = save;
		else
			++n;
		js_pop(J, 1);
	}
	if (gap && n) fmtindent(J, sb, gap, level);
	js_putc(J, sb, '}');
}

static void fmtarray(js_State *J, js_Buffer **sb, const char *gap, int level)
{
	int n, k;
	char buf[32];

	n = js_getlength(J, -1);

	js_putc(J, sb, '[');
	for (k = 0; k < n; ++k) {
		if (k) js_putc(J, sb, ',');
		if (gap) fmtindent(J, sb, gap, level + 1);
		js_itoa(buf, k);
		js_getproperty(J, -1, buf);
		if (!fmtvalue(J, sb, js_intern(J, buf), gap, level + 1))
			js_puts(J, sb, "null");
		js_pop(J, 1);
	}
	if (gap && n) fmtindent(J, sb, gap, level);
	js_putc(J, sb, ']');
}

static int fmtvalue(js_State *J, js_Buffer **sb, const char *key, const char *gap, int level)
{
	if (js_try(J)) {
		js_free(J, *sb);
		js_throw(J);
	}
	if (js_isobject(J, -1)) {
		if (js_hasproperty(J, -1, "toJSON")) {
			if (js_iscallable(J, -1)) {
				js_copy(J, -2);
				js_pushliteral(J, key);
				js_call(J, 1);
				js_rot2pop1(J);
			} else {
				js_pop(J, 1);
			}
		}
	}
	js_endtry(J);

	// TODO: replacer()

	if (js_isobject(J, -1) && !js_iscallable(J, -1)) {
		js_Object *obj = js_toobject(J, -1);
		switch (obj->type) {
		case JS_CNUMBER: fmtnum(J, sb, obj->u.number); break;
		case JS_CSTRING: fmtstr(J, sb, obj->u.s.string); break;
		case JS_CBOOLEAN: js_puts(J, sb, obj->u.boolean ? "true" : "false"); break;
		case JS_CARRAY: fmtarray(J, sb, gap, level); break;
		default: fmtobject(J, sb, obj, gap, level); break;
		}
	}
	else if (js_isboolean(J, -1))
		js_puts(J, sb, js_toboolean(J, -1) ? "true" : "false");
	else if (js_isnumber(J, -1))
		fmtnum(J, sb, js_tonumber(J, -1));
	else if (js_isstring(J, -1))
		fmtstr(J, sb, js_tostring(J, -1));
	else if (js_isnull(J, -1))
		js_puts(J, sb, "null");
	else
		return 0;

	return 1;
}

static void JSON_stringify(js_State *J)
{
	js_Buffer *sb = NULL;
	char buf[12];
	const char *s, *gap;
	int n;

	gap = NULL;

	if (js_isnumber(J, 3)) {
		n = js_tointeger(J, 3);
		if (n < 0) n = 0;
		if (n > 10) n = 10;
		memset(buf, ' ', n);
		buf[n] = 0;
		if (n > 0) gap = buf;
	} else if (js_isstring(J, 3)) {
		s = js_tostring(J, 3);
		n = strlen(s);
		if (n > 10) n = 10;
		memcpy(buf, s, n);
		buf[n] = 0;
		if (n > 0) gap = buf;
	}

	// TODO: replacer

	if (js_isdefined(J, 1)) {
		js_copy(J, 1);
		if (fmtvalue(J, &sb, "", gap, 0)) {
			js_putc(J, &sb, 0);
			if (js_try(J)) {
				js_free(J, sb);
				js_throw(J);
			}
			js_pushstring(J, sb ? sb->s : "");
			js_endtry(J);
			js_free(J, sb);
		}
	} else {
		js_pushundefined(J);
	}
}

void jsB_initjson(js_State *J)
{
	js_pushobject(J, jsV_newobject(J, JS_CJSON, J->Object_prototype));
	{
		jsB_propf(J, "JSON.parse", JSON_parse, 2);
		jsB_propf(J, "JSON.stringify", JSON_stringify, 3);
	}
	js_defglobal(J, "JSON", JS_DONTENUM);
}
