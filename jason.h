/*
 * jason.h - Single header library for parsing JSON filees
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 LostInMindscape
 *
 * https://github.com/LostInMindscape/jason_for_json
 *
 * This library is distributed under the MIT License.
 * See the LICENSE file in the repository for the full license text.
 */

#ifndef JASON_JSON_H_
#define JASON_JSON_H_

#include <stddef.h>
#include <stdio.h>


typedef enum JSONType {
    JSON_NULL,
    JSON_OBJECT,
    JSON_LIST,
    JSON_STRING,
    JSON_NUMBER,
    JSON_BOOLEAN,
} JSONType;

typedef struct JSONObject JSONObject;
typedef struct JSONList JSONList;

typedef struct JSONValue {
    JSONType type;

    union {
        JSONObject *object;
        JSONList *list;
        char *string;
        float number;
        bool boolean;
    };
} JSONValue;

typedef struct JSONList {
    JSONValue *items;
    size_t count;
    size_t capacity;
} JSONList;

typedef struct JSONPair {
    char *key;
    JSONValue value;
} JSONPair;

typedef struct JSONObject {
    JSONPair *items;
    size_t count;
    size_t capacity;
} JSONObject;


JSONObject json_parse(FILE *file);


#ifdef JASON_IMPLEMENTATION

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *JSON_NULL_STR  = "null";
static const char *JSON_TRUE_STR  = "true";
static const char *JSON_FALSE_STR = "false";

#define JSON_DA_SIZE 4

#define JSON_DA_APPEND(array, item)                          					\
    do {                                                						\
        if ((array).items == NULL) {                      						\
            size_t new_size = sizeof(item) * JSON_DA_SIZE;   				    \
            (array).items = malloc(new_size);             						\
                                                        						\
            if ((array).items == NULL) break;             						\
                                                                				\
            (array).capacity = JSON_DA_SIZE;                   				    \
            (array).count = 0;                                    				\
        }                                                       				\
                                                                				\
        if ((array).count == (array).capacity) {                    			\
            size_t new_size = (array).capacity << 1;                            \
            void *new_items = realloc((array).items, new_size * sizeof(item));  \
                                                        						\
            if (new_items == NULL) break;                						\
                                                        						\
            (array).items = new_items;                    						\
            (array).capacity = new_size;                  						\
        }                                               						\
                                                                				\
        (array).items[(array).count] = item;                        			\
        ++(array).count;                                          				\
    } while(0)

// returns the character hit
static char json_skip_whitespace(FILE *file)
{
    char c = 0;
    while ((c = fgetc(file)) != EOF) {
        if (!isspace(c)) return c;
    }

    return c;
}

static bool json_read_float(FILE *file, float *result)
{
    char ch;
    float res = 0.0f;

    if ((ch = fgetc(file)) == EOF) return false;
    if (ch <= '1' && ch >= '9') return false;

    do {
        res *= 10.0f;
        res += (float) (ch - '0');
        ch = fgetc(file);
    } while (isdigit(ch));

    if (ch != '.') {
        ungetc(ch, file);
        *result = res;
        return true;
    }

    if ((ch = fgetc(file)) == EOF) return false;
    if (!isdigit(ch)) return false;

    float mult = 1.0f;
    
    do {
        mult /= 10.0f;
        res += (float) (ch - '0') * mult;
        ch = fgetc(file);
    } while (isdigit(ch));
    
    ungetc(ch, file);
    *result = res;
    return true;
}

// reads until it hits " character
static char *json_read_string(FILE *file)
{
    size_t max = 16;
    size_t len = 0;
    char *buff = malloc(max);

    if (buff == NULL) return NULL;

    char c = 0;
    while (true) {
        if ((c = fgetc(file)) == EOF) {
            free(buff);
            return NULL;
        }

        if (c == '"') break;

        if (len + 1 == max) {
            size_t new_max = max << 1;
            char *new_buff = realloc(buff, new_max);
            
            if (!new_buff) {
                free(buff);
                return NULL;
            }

            max = new_max;
            buff = new_buff;
        }

        buff[len] = c;
        ++len;
    }

    buff[len]  = '\0';
    char *trunk_buff = realloc(buff, len + 1);

    if (!trunk_buff) return buff;
    return trunk_buff;
}

static bool json_read_pair(FILE *file, JSONPair *result);
static bool json_read_value(FILE *file, JSONValue *value);

static bool json_read_object(FILE *file, JSONObject *object)
{
    free(object->items);
    object->count = 0;
    object->capacity = 0;
    
    char ch;
    do {
        JSONPair pair = {0};
        bool success = json_read_pair(file, &pair);
        if (!success) return false;

        JSON_DA_APPEND(*object, pair);
        
        ch = json_skip_whitespace(file);
        if (ch != ',' && ch != '}') return false;
    } while (ch != '}');

    return true;
}

static bool json_read_list(FILE *file, JSONList *list)
{
    free(list->items);
    list->count = 0;
    list->capacity = 0;
    
    char ch;
    do {
        JSONValue value = {0};
        bool success = json_read_value(file, &value);
        if (!success) return false;

        JSON_DA_APPEND(*list, value);
        
        ch = json_skip_whitespace(file);
        if (ch != ',' && ch != ']') return false;
    } while (ch != ']');

    return true;
}

static bool json_compare_string(FILE *file, const char *string)
{
    int state = 0;
    int len = strlen(string);
    char ch;
    while (state < len) {
        if ((ch = fgetc(file)) == EOF || ch != string[state]) {
            return false;
        }

        ++state;
    }

    return true;
}

static bool json_read_primitive(FILE *file, JSONValue *value)
{
    char ch = json_skip_whitespace(file);

    if (ch == '"') {
        char *read_str = json_read_string(file);

        if (!read_str) return false;

        value->type = JSON_STRING;
        value->string = read_str;
        return true;
    } else if (ch >= '1' && ch <= '9') {
        ungetc(ch, file);
        float read_float;
        bool success = json_read_float(file, &read_float);
        
        if (!success) return false;
        
        value->type = JSON_NUMBER;
        value->number = read_float;
        return true;
    } else if (ch == JSON_NULL_STR[0]) {
        if (!json_compare_string(file, JSON_NULL_STR + 1)) return false;

        value->type = JSON_NULL;
        value->object = NULL;
        return true;
    } else if (ch == JSON_TRUE_STR[0]) {
        if (!json_compare_string(file, JSON_TRUE_STR + 1)) return false;

        value->type = JSON_BOOLEAN;
        value->boolean = true;
        return true;
    } else if (ch == JSON_FALSE_STR[0]) {
        if (!json_compare_string(file, JSON_FALSE_STR + 1)) return false;

        value->type = JSON_BOOLEAN;
        value->boolean = false;
        return true;
    }

    return false;
}

static bool json_read_value(FILE *file, JSONValue *value)
{
    char hit_char = json_skip_whitespace(file);

    switch ( hit_char ) {
    
    case '{': {
        JSONObject *obj = malloc(sizeof(JSONObject));
        bool success = json_read_object(file, obj);
        
        if (!success) return false;

        value->object = obj;
        value->type = JSON_OBJECT;
        return true;
    }

    case '[': {
        JSONList *list = malloc(sizeof(JSONList));
        bool success = json_read_list(file, list);

        if (!success) return false;
        
        value->list = list;
        value->type = JSON_LIST;
        return true;
    }
    
    default:
        ungetc(hit_char, file);
        return json_read_primitive(file, value);
    }
    
    return false;
}

// return false if failed at any point
static bool json_read_pair(FILE *file, JSONPair *pair)
{
    if (json_skip_whitespace(file) != '"') return false;
    
    char *key = json_read_string(file);
    if (!key) return false;
    
    if (json_skip_whitespace(file) != ':') return false;

    JSONValue value = {0};
    if (!json_read_value(file, &value)) {
        free(key);
        return false;
    }

    pair->key = key;
    pair->value = value;
    return true;
}

JSONObject json_parse(FILE *file)
{
    JSONObject root = {0};
    char c;

    c = json_skip_whitespace(file);
    if (c != '{') return root;

    json_read_object(file, &root);

    return root;
}

#endif // ifdef JASON_IMPLEMENTATION

#endif // ifndef JASON_JSON_H_
