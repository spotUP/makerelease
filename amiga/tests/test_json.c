#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_STR(a, b) do { \
    if (strcmp((a), (b)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, (a), (b)); failures++; } \
} while (0)

static void test_valid(void) {
    char err[256];
    jnode *n = json_parse("{\"label\":\"Up Rough\",\"n\":5,\"ok\":true,\"off\":false,\"nil\":null,"
                          "\"members\":[{\"nickname\":\"coder\",\"role\":\"code\"}],\"e\":\"A\\u00e9\\n\"}",
                          err, sizeof(err));
    const jnode *m;
    CHECK(n != NULL);
    CHECK(n->type == J_OBJ);
    CHECK_STR(json_get(n, "label")->str, "Up Rough");
    CHECK(json_get(n, "n")->num == 5);
    CHECK(json_get(n, "ok")->num == 1);
    CHECK(json_get(n, "off")->num == 0);
    CHECK(json_get(n, "nil")->type == J_NULL);
    m = json_get(n, "members");
    CHECK(m != NULL && m->type == J_ARR && m->n == 1);
    CHECK(m->items[0]->type == J_OBJ);
    CHECK_STR(json_get(json_get(n, "members")->items[0], "role")->str, "code");
    CHECK_STR(json_get(n, "e")->str, "A\xe9\n");
    json_free(n);
}

static void test_errors(void) {
    char err[256];
    jnode *n;
    n = json_parse("{\"a\":1} trailing", err, sizeof(err));
    CHECK(n == NULL);
    n = json_parse("{\"a\":}", err, sizeof(err));
    CHECK(n == NULL);
    n = json_parse("{\"a\"", err, sizeof(err));
    CHECK(n == NULL);
    n = json_parse("{\"a\":\"\\q\"}", err, sizeof(err));
    CHECK(n == NULL);
    n = json_parse("{\"a\":\"\\u1234\"}", err, sizeof(err));
    CHECK(n == NULL);
    n = json_parse("{\"a\":1,\"a\":2}", err, sizeof(err));
    CHECK(n == NULL);
    n = json_parse("{\"a\":1.5}", err, sizeof(err));
    CHECK(n == NULL);
    n = json_parse("[1,]", err, sizeof(err));
    CHECK(n == NULL); /* no trailing commas, like the desktop parser */
}

int main(void) {
    test_valid();
    test_errors();
    if (failures == 0) puts("json: all passed");
    return failures != 0;
}
