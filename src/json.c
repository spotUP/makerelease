#include "json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    const char *p;
    char *err;
    size_t errcap;
} cursor;

/* Byte offsets are reported from the caller-known base. */
static void fail_at(cursor *c, const char *base, const char *at, const char *msg) {
    snprintf(c->err, c->errcap, "JSON error at byte %ld: %s", (long)(at - base), msg);
}

static void skip_ws(cursor *c) {
    while (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r') c->p++;
}

static jnode *new_node(jtype type) {
    jnode *n = calloc(1, sizeof *n);
    if (n) n->type = type;
    return n;
}

static void free_node(jnode *n) {
    size_t i;
    if (!n) return;
    free(n->str);
    for (i = 0; i < n->n; i++) {
        free(n->keys ? n->keys[i] : NULL);
        free_node(n->items[i]);
    }
    free(n->keys);
    free(n->items);
    free(n);
}

void json_free(jnode *n) {
    free_node(n);
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parses a string starting at the opening quote; leaves *c->p past the end. */
static char *parse_string(cursor *c, const char *base) {
    char *out, *w;
    size_t cap = 32, len = 0;
    c->p++; /* opening quote */
    out = malloc(cap);
    if (!out) return NULL;
    w = out;
    for (;;) {
        char ch = *c->p;
        if (ch == '\0') {
            fail_at(c, base, c->p, "unterminated string");
            free(out);
            return NULL;
        }
        if (ch == '"') {
            c->p++;
            break;
        }
        if (ch == '\\') {
            int h1, h2, h3, h4, cp;
            c->p++;
            switch (*c->p) {
            case '"': ch = '"'; c->p++; break;
            case '\\': ch = '\\'; c->p++; break;
            case '/': ch = '/'; c->p++; break;
            case 'b': ch = '\b'; c->p++; break;
            case 'f': ch = '\f'; c->p++; break;
            case 'n': ch = '\n'; c->p++; break;
            case 'r': ch = '\r'; c->p++; break;
            case 't': ch = '\t'; c->p++; break;
            case 'u':
                h1 = hex_val(c->p[1]);
                h2 = hex_val(c->p[2]);
                h3 = hex_val(c->p[3]);
                h4 = hex_val(c->p[4]);
                if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) {
                    fail_at(c, base, c->p, "bad \\u escape");
                    free(out);
                    return NULL;
                }
                cp = (h1 << 12) | (h2 << 8) | (h3 << 4) | h4;
                if (cp > 0xFF) {
                    fail_at(c, base, c->p, "only \\u00XX-\\u00FF fit Latin-1");
                    free(out);
                    return NULL;
                }
                ch = (char)cp;
                c->p += 5;
                break;
            default:
                fail_at(c, base, c->p, "bad escape");
                free(out);
                return NULL;
            }
        } else {
            if ((unsigned char)ch < 0x20) {
                fail_at(c, base, c->p, "control character in string");
                free(out);
                return NULL;
            }
            c->p++;
        }
        if (len + 1 >= cap) {
            char *n;
            cap *= 2;
            n = realloc(out, cap);
            if (!n) {
                free(out);
                return NULL;
            }
            w = n + len;
            out = n;
        }
        *w++ = ch;
        len++;
    }
    *w = '\0';
    return out;
}

static jnode *parse_value(cursor *c, const char *base);

static jnode *parse_array(cursor *c, const char *base) {
    jnode *n = new_node(J_ARR);
    if (!n) return NULL;
    c->p++; /* [ */
    skip_ws(c);
    if (*c->p == ']') {
        c->p++;
        return n;
    }
    for (;;) {
        jnode *item = parse_value(c, base);
        jnode **ni;
        if (!item) {
            free_node(n);
            return NULL;
        }
        ni = realloc(n->items, (n->n + 1) * sizeof *ni);
        if (!ni) {
            free_node(item);
            free_node(n);
            return NULL;
        }
        n->items = ni;
        n->items[n->n++] = item;
        skip_ws(c);
        if (*c->p == ',') {
            c->p++;
            skip_ws(c);
            continue;
        }
        if (*c->p == ']') {
            c->p++;
            return n;
        }
        fail_at(c, base, c->p, "expected ',' or ']'");
        free_node(n);
        return NULL;
    }
}

static jnode *parse_object(cursor *c, const char *base) {
    jnode *n = new_node(J_OBJ);
    if (!n) return NULL;
    c->p++; /* { */
    skip_ws(c);
    if (*c->p == '}') {
        c->p++;
        return n;
    }
    for (;;) {
        char *key;
        jnode *val;
        char **nk;
        jnode **ni;
        size_t i;
        if (*c->p != '"') {
            fail_at(c, base, c->p, "expected a key string");
            free_node(n);
            return NULL;
        }
        key = parse_string(c, base);
        if (!key) {
            free_node(n);
            return NULL;
        }
        for (i = 0; i < n->n; i++) {
            if (strcmp(n->keys[i], key) == 0) {
                fail_at(c, base, c->p, "duplicate key");
                free(key);
                free_node(n);
                return NULL;
            }
        }
        skip_ws(c);
        if (*c->p != ':') {
            fail_at(c, base, c->p, "expected ':'");
            free(key);
            free_node(n);
            return NULL;
        }
        c->p++;
        skip_ws(c);
        val = parse_value(c, base);
        if (!val) {
            free(key);
            free_node(n);
            return NULL;
        }
        nk = realloc(n->keys, (n->n + 1) * sizeof *nk);
        ni = realloc(n->items, (n->n + 1) * sizeof *ni);
        if (!nk || !ni) {
            free(nk);
            free(ni);
            free(key);
            free_node(val);
            free_node(n);
            return NULL;
        }
        n->keys = nk;
        n->items = ni;
        n->keys[n->n] = key;
        n->items[n->n] = val;
        n->n++;
        skip_ws(c);
        if (*c->p == ',') {
            c->p++;
            skip_ws(c);
            continue;
        }
        if (*c->p == '}') {
            c->p++;
            return n;
        }
        fail_at(c, base, c->p, "expected ',' or '}'");
        free_node(n);
        return NULL;
    }
}

static jnode *parse_number(cursor *c, const char *base) {
    const char *start = c->p;
    long v = 0;
    int neg = 0;
    jnode *n;
    if (*c->p == '-') {
        neg = 1;
        c->p++;
    }
    if (*c->p < '0' || *c->p > '9') {
        fail_at(c, base, start, "bad number");
        return NULL;
    }
    while (*c->p >= '0' && *c->p <= '9') {
        v = v * 10 + (*c->p - '0');
        c->p++;
    }
    if (*c->p == '.' || *c->p == 'e' || *c->p == 'E') {
        fail_at(c, base, start, "only integers are supported");
        return NULL;
    }
    n = new_node(J_NUM);
    if (n) n->num = neg ? -v : v;
    return n;
}

static jnode *parse_value(cursor *c, const char *base) {
    jnode *n;
    skip_ws(c);
    switch (*c->p) {
    case '{': return parse_object(c, base);
    case '[': return parse_array(c, base);
    case '"':
        n = new_node(J_STR);
        if (!n) return NULL;
        n->str = parse_string(c, base);
        if (!n->str) {
            free(n);
            return NULL;
        }
        return n;
    case 't':
        if (strncmp(c->p, "true", 4) == 0) {
            c->p += 4;
            n = new_node(J_BOOL);
            if (n) n->num = 1;
            return n;
        }
        break;
    case 'f':
        if (strncmp(c->p, "false", 5) == 0) {
            c->p += 5;
            n = new_node(J_BOOL);
            return n; /* num 0 */
        }
        break;
    case 'n':
        if (strncmp(c->p, "null", 4) == 0) {
            c->p += 4;
            return new_node(J_NULL);
        }
        break;
    default: break;
    }
    if ((*c->p >= '0' && *c->p <= '9') || *c->p == '-') return parse_number(c, base);
    fail_at(c, base, c->p, "unexpected character");
    return NULL;
}

jnode *json_parse(const char *text, char *err, size_t errcap) {
    cursor c;
    jnode *n;
    c.p = text;
    c.err = err;
    c.errcap = errcap;
    if (errcap > 0) err[0] = '\0';
    n = parse_value(&c, text);
    if (!n) return NULL;
    skip_ws(&c);
    if (*c.p != '\0') {
        fail_at(&c, text, c.p, "trailing characters");
        free_node(n);
        return NULL;
    }
    return n;
}

const jnode *json_get(const jnode *obj, const char *key) {
    size_t i;
    if (!obj || obj->type != J_OBJ) return NULL;
    for (i = 0; i < obj->n; i++) {
        if (strcmp(obj->keys[i], key) == 0) return obj->items[i];
    }
    return NULL;
}
