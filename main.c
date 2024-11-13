#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FILE_PATH "test1.json"

typedef enum {
    JT_NULL,
    JT_NUMBER,
    JT_STRING,
    JT_BOOLEAN,
    JT_COMMA,
    JT_COLON,
    JT_OBRACE,
    JT_CBRACE,
    JT_OBRACKET,
    JT_CBRACKET
} JTKind;

#define array_append(mem, arr, val)                                            \
    do {                                                                       \
        if ((arr)->size == 0) {                                                \
            (arr)->size = 8;                                                   \
            (arr)->pool = jalloc((mem), (arr)->size * sizeof(*((arr)->pool))); \
        } else if ((arr)->count == (arr)->size) {                              \
            if ((arr)->pool ==                                                 \
                &(mem->pool[(mem)->count -                                     \
                            (arr)->size * sizeof(*((arr)->pool))])) {          \
                jalloc((mem), (arr)->size * sizeof(*((arr)->pool)));           \
                (arr)->size *= 2;                                              \
                printf("fast extend\n");                                       \
            } else {                                                           \
                void *new =                                                    \
                    jalloc((mem), 2 * (arr)->size * sizeof(*((arr)->pool)));   \
                memcpy(new, (arr)->pool,                                       \
                       (arr)->size * sizeof(*((arr)->pool)));                  \
                (arr)->size *= 2;                                              \
                ((arr)->pool) = new;                                           \
                printf("slow extend\n");                                       \
            }                                                                  \
        }                                                                      \
        (arr)->pool[(arr)->count++] = (val);                                   \
    } while (0)

typedef struct SV {
    uint64_t size;
    char *str;
} SV;

int SV_cmp(SV sv1, SV sv2) {
    if (sv1.size != sv2.size)
        return -1;
    if (sv1.str[0] != sv2.str[0])
        return -1;
    for (int i = 1; i < sv1.size; i++) {
        if (sv1.str[i] != sv2.str[i])
            return -1;
    }
    return 0;
}

typedef struct JTOK {
    JTKind kind;
    SV tok;
} JTOK;

typedef struct JTOKS {
    uint64_t count;
    uint64_t size;
    JTOK *pool;
} JTOKS;

typedef struct JArena {
    uint64_t size;
    uint64_t count;
    void *pool;
} JArena;

#define JDEFAULT_POOL_SIZE 8 * 1024

JArena createArena(void) {
    JArena a = {0};
    a.size = JDEFAULT_POOL_SIZE;
    a.pool = calloc(JDEFAULT_POOL_SIZE, 1);
    return a;
}
void freeArena(JArena *arena) {
    free(arena->pool);
    arena->pool = NULL;
    arena->count = 0;
    arena->size = 0;
}

void *jalloc(JArena *arena, uint64_t size) {
    assert((arena->size > 0) && "ERROR: arena not initialize\n");
    assert((arena->count < arena->size) &&
           "ERROR: lul no memory buy more ram\n");
    assert((arena->count + size <= arena->size) &&
           "ERROR: not enough memory\n");

    char *ret = &((char *)arena->pool)[arena->count];
    arena->count += size;
    return ret;
}

JTOKS jtokenize(JArena *mem, char *str, uint64_t len) {
    JTOKS toks = {0};
    uint64_t idx = 0;
    while (idx < len) {
        char c = str[idx];

        if (isspace(c)) {
            // pass;
        } else if (c == '{') {
            JTOK tok = {.kind = JT_OBRACE,
                        .tok = {.str = &str[idx], .size = 1}};
            array_append(mem, &toks, tok);

        } else if (c == '}') {
            JTOK tok = {.kind = JT_CBRACE,
                        .tok = {.str = &str[idx], .size = 1}};
            array_append(mem, &toks, tok);
        } else if (c == '"') {
            JTOK tok = {.kind = JT_STRING,
                        .tok = {.str = &str[idx], .size = 1}};
            idx++;
            while (str[idx] != '"') {
                tok.tok.size++;
                idx++;
            }
            tok.tok.size++;
            array_append(mem, &toks, tok);
        } else if (c == 't' || c == 'f') {
            JTOK tok = {.kind = JT_BOOLEAN,
                        .tok = {.str = &str[idx], .size = 4}};
            if (c == 'f')
                tok.tok.size++;
            if (!SV_cmp(tok.tok, (SV){.str = "true", .size = strlen("true")})) {
                array_append(mem, &toks, tok);
                idx += 3;
            } else if (!SV_cmp(tok.tok,
                               (SV){.str = "false", .size = strlen("false")})) {
                tok.tok.size++;
                array_append(mem, &toks, tok);
                idx += 4;
            } else {
                assert(0 && "ERROR: UNKNOWN TOKEN");
            }
        } else if (isdigit(c)) {
            // TODO: detect other type of number
            // current only support positive integer
            JTOK tok = {.kind = JT_NUMBER,
                        .tok = {.size = 1, .str = &str[idx]}};
            idx++;
            while (isdigit(str[idx])) {
                tok.tok.size++;
                idx++;
            }
            idx++;
            array_append(mem, &toks, tok);
        } else if (c == ',') {
            JTOK tok = {.kind = JT_COMMA, .tok = {.size = 1, .str = &str[idx]}};
            array_append(mem, &toks, tok);
        } else if (c == ':') {
            JTOK tok = {.kind = JT_COLON, .tok = {.size = 1, .str = &str[idx]}};
            array_append(mem, &toks, tok);
        } else {
        }
        idx++;
    }

    for (int i = 0; i < toks.count; i++) {
        printf("JTOK( kind: %d, str: %.*s)\n", toks.pool[i].kind,
               (int)toks.pool[i].tok.size, toks.pool[i].tok.str);
    }
    return toks;
}

typedef union JData JData;
typedef struct JObject JObject;
typedef union {
    int32_t integer;
    float decimal;
} JNumber;

union JData {
    SV str;
    JNumber num;
    _Bool boolean;
    JObject *obj;
};
typedef enum {
    JV_NUMBER,
    JV_BOOL,
    JV_STRING,
    JV_OBJ,
    JV_ARR,
    JV_NULL
} JVKind;
typedef struct JValue {
    JVKind kind;
    JData as;
} JValue;
typedef struct {
    SV key;
    JValue val;
} JKV;

struct JObject {
    uint64_t count, size;
    JKV *pool;
};
typedef struct {
    JValue data;
    JArena arena;
} Json;

typedef struct {
    JTOKS toks;
    int idx;
    JArena *arena;
} JParser;
// JValue parseJObject(JArena *arena, JTOKS tok, uint64_t start) {
//     JValue val = {0};
//     val.kind = JV_OBJ;
//     JTOK ctok = tok.pool[start];
//     if (ctok.kind == JT_OBRACE) {
//         start++;
//     } else {
//         assert(0 && "invalid jobject");
//     }
// }

JValue parseJValue(JParser *parser) {

    JTOK ctok = parser->toks.pool[parser->idx];
    JValue val = {0};
    switch (ctok.kind) {
    case JT_STRING: {
        val.kind = JV_STRING;
        val.as.str = ctok.tok;
        break;
    }
    case JT_NUMBER: {
        val.kind = JV_NUMBER;
        char *temp = jalloc(parser->arena, ctok.tok.size + 1);
        strncpy(temp, ctok.tok.str, ctok.tok.size);
        val.as.num.integer = atoi(temp);
        break;
    }
    case JT_BOOLEAN: {
        val.kind = JV_BOOL;
        if (!SV_cmp(ctok.tok, (SV){.str = "true", .size = 4})) {
            val.as.boolean = 1;
        } else {
            val.as.boolean = 0;
        }
        break;
    }
    // case JT_OBRACE: {
    //     val = parseJObject(arena, tok, start) break;
    // }
    default:
        assert(0 && "Unreachable parseJValue");
        break;
    }
    return val;
}
Json parseJson(const char *jsonstr, int len) {
    Json json = {0};
    json.arena = createArena();
    char *content = jalloc(&json.arena, len);
    strncpy(content, jsonstr, len);
    content[len - 1] = 0;
    printf("%s\n", content);
    JTOKS toks = jtokenize(&json.arena, content, len);
    JParser parser = {.toks = toks, .idx = 0, .arena = &json.arena};
    json.data = parseJValue(&parser);
    return json;
}

void debugPrint(JValue val) {
    switch (val.kind) {
    case JV_STRING:
        printf("%.*s", (int)val.as.str.size, val.as.str.str);
        break;
    case JV_NUMBER:
        printf("%d", val.as.num.integer);
        break;
    case JV_BOOL:
        printf("%s", val.as.boolean ? "true" : "false");
        break;
    default:
        assert(0 && "Unreachable parseJValue");
        break;
    }
}
void freeJson(Json *json) {
    freeArena(&json->arena);
    memset(json, 0, sizeof(*json));
}

int main(int argc, char **argv) {
    FILE *f = fopen(FILE_PATH, "r");

    fseek(f, 0, SEEK_END);
    uint64_t len = ftell(f) + 1;
    fseek(f, 0, SEEK_SET);

    char *content = calloc(1, len);
    fread(content, sizeof(*content), len, f);
    fclose(f);
    Json json = parseJson(content, len);
    free(content);
    debugPrint(json.data);
    freeJson(&json);

    return 0;
}
