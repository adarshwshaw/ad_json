#ifndef AD_JSON_H
#define AD_JSON_H


#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AD_JSON_LIB_MOD
#define AD_JSON_API 
#else
#define AD_JSON_API static
#endif


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

#define SV_FMT "%.*s"
#define SV_ARG(X) (int)((X).size), (X).str

AD_JSON_API int SV_cmp(SV sv1, SV sv2);

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

AD_JSON_API JArena createArena(void);
AD_JSON_API void freeArena(JArena *arena);
AD_JSON_API void *jalloc(JArena *arena, uint64_t size);
AD_JSON_API JTOKS jtokenize(JArena *mem, char *str, uint64_t len);

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

AD_JSON_API JTOK peekTok(JParser *parser);

#define consumeTok(parser) (parser)->idx++
AD_JSON_API JValue parseJValue(JParser *parser);
AD_JSON_API JKV parseJKV(JParser *parser);
AD_JSON_API JValue parseJObject(JParser *parser);
AD_JSON_API Json parseJson(const char *jsonstr, int len);
AD_JSON_API void debugPrint(JValue val);
AD_JSON_API void freeJson(Json *json);

#ifdef AD_JSON_IMPLEMENTATION

AD_JSON_API int SV_cmp(SV sv1, SV sv2) {
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

AD_JSON_API JArena createArena(void) {
    JArena a = {0};
    a.size = JDEFAULT_POOL_SIZE;
    a.pool = calloc(JDEFAULT_POOL_SIZE, 1);
    return a;
}
AD_JSON_API void freeArena(JArena *arena) {
    free(arena->pool);
    arena->pool = NULL;
    arena->count = 0;
    arena->size = 0;
}

AD_JSON_API void *jalloc(JArena *arena, uint64_t size) {
    assert((arena->size > 0) && "ERROR: arena not initialize\n");
    assert((arena->count < arena->size) &&
           "ERROR: lul no memory buy more ram\n");
    assert((arena->count + size <= arena->size) &&
           "ERROR: not enough memory\n");

    char *ret = &((char *)arena->pool)[arena->count];
    arena->count += size;
    return ret;
}
AD_JSON_API JTOKS jtokenize(JArena *mem, char *str, uint64_t len) {
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
            idx--;
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
AD_JSON_API JTOK peekTok(JParser *parser) { return parser->toks.pool[parser->idx]; }

AD_JSON_API JKV parseJKV(JParser *parser) {
    JKV jkv = {0};
    JTOK ctok = peekTok(parser);
    if (ctok.kind == JT_STRING) {
        consumeTok(parser);
        jkv.key = ctok.tok;
        jkv.key.size -= 2;
        jkv.key.str++;
    } else {
        printf(SV_FMT, SV_ARG(ctok.tok));
        assert(0);
    }
    ctok = peekTok(parser);
    if (ctok.kind == JT_COLON) {
        consumeTok(parser);
    } else {
        assert(0);
    }
    jkv.val = parseJValue(parser);
    return jkv;
}
AD_JSON_API JValue parseJObject(JParser *parser) {
    JValue val = {0};
    val.kind = JV_OBJ;
    val.as.obj = jalloc(parser->arena, sizeof(JObject));
    JTOK ctok = peekTok(parser);
    if (ctok.kind == JT_OBRACE) {
        consumeTok(parser);
    } else {
        assert(0 && "invalid jobject");
    }
    ctok = peekTok(parser);
    while (ctok.kind != JT_CBRACE) {
        JKV jkv = parseJKV(parser);
        array_append(parser->arena, val.as.obj, jkv);
        ctok = peekTok(parser);
        if (ctok.kind == JT_COMMA) {
            consumeTok(parser);
            ctok = peekTok(parser);
            continue;
        } else if (ctok.kind == JT_CBRACE) {
            continue;
        } else {
            printf(SV_FMT, SV_ARG(ctok.tok));
            assert(0 && "unreachable parseobject");
        }
    }
    return val;
}

AD_JSON_API JValue parseJValue(JParser *parser) {

    JTOK ctok = parser->toks.pool[parser->idx];
    JValue val = {0};
    switch (ctok.kind) {
    case JT_STRING: {
        val.kind = JV_STRING;
        val.as.str = ctok.tok;
        val.as.str.str++;
        val.as.str.size -= 2;
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
    case JT_OBRACE: {
        val = parseJObject(parser);
        break;
    }
    default:
        printf(SV_FMT, SV_ARG(ctok.tok));
        assert(0 && "Unreachable parseJValue");
        break;
    }
    consumeTok(parser);
    return val;
}
AD_JSON_API Json parseJson(const char *jsonstr, int len) {
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

AD_JSON_API void debugPrint(JValue val) {
    switch (val.kind) {
    case JV_STRING:
        printf(SV_FMT, SV_ARG(val.as.str));
        break;
    case JV_NUMBER:
        printf("%d", val.as.num.integer);
        break;
    case JV_BOOL:
        printf("%s", val.as.boolean ? "true" : "false");
        break;
    case JV_OBJ:
        for (int i = 0; i < val.as.obj->count; i++) {
            printf(SV_FMT " : ", SV_ARG(val.as.obj->pool[i].key));
            debugPrint(val.as.obj->pool[i].val);
            printf("\n");
        }
        break;
    default:
        assert(0 && "Unreachable parseJValue");
        break;
    }
}
AD_JSON_API void freeJson(Json *json) {
    printf("mem used : %lld\n", json->arena.count);
    freeArena(&json->arena);
    memset(json, 0, sizeof(*json));
}
#endif //AD_JSON_IMPLEMENTATION
#endif //AD_JOSN_H
