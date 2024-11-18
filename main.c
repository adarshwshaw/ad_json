#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define AD_JSON_IMPLEMENTATION
#include "ad_json.h"
#define FILE_PATH "test.json"

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
