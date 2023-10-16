/*
  Copyright (c) 2009-2017 Dave Gamble and struct json_node contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef _UAPI_LIB_JSON_CORE_H_
#define _UAPI_LIB_JSON_CORE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define CJSON_CDECL

/* project version */
#define CJSON_VERSION_MAJOR 1
#define CJSON_VERSION_MINOR 7
#define CJSON_VERSION_PATCH 16

#include <stddef.h>
#include <stdbool.h>

/* struct json_node Types: */
#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7) /* raw json */

#define cJSON_IsReference		(1 << 8)
#define cJSON_StringIsConst		(1 << 9)

/* The struct json_node structure: */
struct json_node {
    /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct json_node *next;
    struct json_node *prev;
    /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
    struct json_node *child;

    /* The type of the item, as above. */
    int type;

    /* The item's string, if type==cJSON_String  and type == cJSON_Raw */
    char *valuestring;
    /* writing to valueint is DEPRECATED, use cJSON_SetNumberValue instead */
    int valueint;
    /* The item's number, if type==cJSON_Number */
    double valuedouble;

    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char *string;
};

typedef struct cJSON_Hooks
{
      /* malloc/free are CDECL on Windows regardless of the default calling convention of the compiler, so ensure the hooks allow passing those functions directly. */
      void *(CJSON_CDECL *malloc_fn)(size_t sz);
      void (CJSON_CDECL *free_fn)(void *ptr);
} cJSON_Hooks;

/* Limits how deeply nested arrays/objects can be before struct json_node rejects to parse them.
 * This is to prevent stack overflows. */
#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

/* returns the version of struct json_node as a string */
const char * cJSON_Version(void);

/* Supply malloc, realloc and free functions to struct json_node */
void cJSON_InitHooks(cJSON_Hooks* hooks);

/* Memory Management: the caller is always responsible to free the results from all variants of cJSON_Parse (with cJSON_Delete) and cJSON_Print (with stdlib free, cJSON_Hooks.free_fn, or cJSON_free as appropriate). The exception is cJSON_PrintPreallocated, where the caller has full responsibility of the buffer. */
/* Supply a block of JSON, and this returns a struct json_node object you can interrogate. */
struct json_node * cJSON_Parse(const char *value);
struct json_node * cJSON_ParseWithLength(const char *value, size_t buffer_length);
/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
/* If you supply a ptr in return_parse_end and parsing fails, then return_parse_end will contain a pointer to the error so will match cJSON_GetErrorPtr(). */
struct json_node * cJSON_ParseWithOpts(const char *value, const char **return_parse_end, bool require_null_terminated);
struct json_node * cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, bool require_null_terminated);

/* Render a struct json_node entity to text for transfer/storage. */
char * cJSON_Print(const struct json_node *item);
/* Render a struct json_node entity to text for transfer/storage without any formatting. */
char * cJSON_PrintUnformatted(const struct json_node *item);
/* Render a struct json_node entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
char * cJSON_PrintBuffered(const struct json_node *item, int prebuffer, bool fmt);
/* Render a struct json_node entity to text using a buffer already allocated in memory with given length. Returns 1 on success and 0 on failure. */
/* NOTE: struct json_node is not always 100% accurate in estimating how much memory it will use, so to be safe allocate 5 bytes more than you actually need */
bool cJSON_PrintPreallocated(struct json_node *item, char *buffer, const int length, const bool format);
/* Delete a struct json_node entity and all subentities. */
void cJSON_Delete(struct json_node *item);

/* Returns the number of items in an array (or object). */
int cJSON_GetArraySize(const struct json_node *array);
/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
struct json_node * cJSON_GetArrayItem(const struct json_node *array, int index);
/* Get item "string" from object. Case insensitive. */
struct json_node * cJSON_GetObjectItem(const struct json_node * const object, const char * const string);
bool cJSON_HasObjectItem(const struct json_node *object, const char *string);
/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */
const char * cJSON_GetErrorPtr(void);

/* Check item type and return its value */
char * cJSON_GetStringValue(const struct json_node * const item);
double cJSON_GetNumberValue(const struct json_node * const item);

/* These functions check the type of an item */
bool cJSON_IsInvalid(const struct json_node * const item);
bool cJSON_IsFalse(const struct json_node * const item);
bool cJSON_IsTrue(const struct json_node * const item);
bool cJSON_IsBool(const struct json_node * const item);
bool cJSON_IsNull(const struct json_node * const item);
bool cJSON_IsNumber(const struct json_node * const item);
bool cJSON_IsString(const struct json_node * const item);
bool cJSON_IsArray(const struct json_node * const item);
bool cJSON_IsObject(const struct json_node * const item);
bool cJSON_IsRaw(const struct json_node * const item);

/* These calls create a struct json_node item of the appropriate type. */
struct json_node * cJSON_CreateNull(void);
struct json_node * cJSON_CreateTrue(void);
struct json_node * cJSON_CreateFalse(void);
struct json_node * cJSON_CreateBool(bool boolean);
struct json_node * cJSON_CreateNumber(double num);
struct json_node * cJSON_CreateString(const char *string);
/* raw json */
struct json_node * cJSON_CreateRaw(const char *raw);
struct json_node * cJSON_CreateArray(void);
struct json_node * cJSON_CreateObject(void);

/* Create a string where valuestring references a string so
 * it will not be freed by cJSON_Delete */
struct json_node * cJSON_CreateStringReference(const char *string);
/* Create an object/array that only references it's elements so
 * they will not be freed by cJSON_Delete */
struct json_node * cJSON_CreateObjectReference(const struct json_node *child);
struct json_node * cJSON_CreateArrayReference(const struct json_node *child);

/* These utilities create an Array of count items.
 * The parameter count cannot be greater than the number of elements in the number array, otherwise array access will be out of bounds.*/
struct json_node * cJSON_CreateIntArray(const int *numbers, int count);
struct json_node * cJSON_CreateFloatArray(const float *numbers, int count);
struct json_node * cJSON_CreateDoubleArray(const double *numbers, int count);
struct json_node * cJSON_CreateStringArray(const char *const *strings, int count);

/* Append item to the specified array/object. */
bool cJSON_AddItemToArray(struct json_node *array, struct json_node *item);
bool cJSON_AddItemToObject(struct json_node *object, const char *string, struct json_node *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing struct json_node to a new struct json_node, but don't want to corrupt your existing struct json_node. */
bool cJSON_AddItemReferenceToArray(struct json_node *array, struct json_node *item);
bool cJSON_AddItemReferenceToObject(struct json_node *object, const char *string, struct json_node *item);

/* Remove/Detach items from Arrays/Objects. */
struct json_node * cJSON_DetachItemViaPointer(struct json_node *parent, struct json_node * const item);
struct json_node * cJSON_DetachItemFromArray(struct json_node *array, int which);
void cJSON_DeleteItemFromArray(struct json_node *array, int which);
struct json_node * cJSON_DetachItemFromObject(struct json_node *object, const char *string);
void cJSON_DeleteItemFromObject(struct json_node *object, const char *string);

/* Update array items. */
bool cJSON_InsertItemInArray(struct json_node *array, int which, struct json_node *newitem); /* Shifts pre-existing items to the right. */
bool cJSON_ReplaceItemViaPointer(struct json_node * const parent, struct json_node * const item, struct json_node * replacement);
bool cJSON_ReplaceItemInArray(struct json_node *array, int which, struct json_node *newitem);
bool cJSON_ReplaceItemInObject(struct json_node *object,const char *string,struct json_node *newitem);

/* Duplicate a struct json_node item */
struct json_node * cJSON_Duplicate(const struct json_node *item, bool recurse);
/* Duplicate will create a new, identical struct json_node item to the one you pass, in new memory that will
 * need to be released. With recurse!=0, it will duplicate any children connected to the item.
 * The item->next and ->prev pointers are always zero on return from Duplicate. */
/* Recursively compare two struct json_node items for equality. If either a or b is NULL or invalid, they will be considered unequal. */
bool cJSON_Compare(const struct json_node * const a, const struct json_node * const b);

/* Minify a strings, remove blank characters(such as ' ', '\t', '\r', '\n') from strings.
 * The input pointer json cannot point to a read-only address area, such as a string constant, 
 * but should point to a readable and writable address area. */
void cJSON_Minify(char *json);

/* Helper functions for creating and adding items to an object at the same time.
 * They return the added item or NULL on failure. */
struct json_node * cJSON_AddNullToObject(struct json_node * const object, const char * const name);
struct json_node * cJSON_AddTrueToObject(struct json_node * const object, const char * const name);
struct json_node * cJSON_AddFalseToObject(struct json_node * const object, const char * const name);
struct json_node * cJSON_AddBoolToObject(struct json_node * const object, const char * const name, const bool boolean);
struct json_node * cJSON_AddNumberToObject(struct json_node * const object, const char * const name, const double number);
struct json_node * cJSON_AddStringToObject(struct json_node * const object, const char * const name, const char * const string);
struct json_node * cJSON_AddRawToObject(struct json_node * const object, const char * const name, const char * const raw);
struct json_node * cJSON_AddObjectToObject(struct json_node * const object, const char * const name);
struct json_node * cJSON_AddArrayToObject(struct json_node * const object, const char * const name);

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define cJSON_SetIntValue(object, number) ((object) ? (object)->valueint = (object)->valuedouble = (number) : (number))
/* helper for the cJSON_SetNumberValue macro */
double cJSON_SetNumberHelper(struct json_node *object, double number);
#define cJSON_SetNumberValue(object, number) ((object != NULL) ? cJSON_SetNumberHelper(object, (double)number) : (number))
/* Change the valuestring of a cJSON_String object, only takes effect when type of object is cJSON_String */
char * cJSON_SetValuestring(struct json_node *object, const char *valuestring);

/* If the object is not a boolean type this does nothing and returns cJSON_Invalid else it returns the new type*/
#define cJSON_SetBoolValue(object, boolValue) ( \
    (object != NULL && ((object)->type & (cJSON_False|cJSON_True))) ? \
    (object)->type=((object)->type &(~(cJSON_False|cJSON_True)))|((boolValue)?cJSON_True:cJSON_False) : \
    cJSON_Invalid\
)

/* Macro for iterating over an array or object */
#define cJSON_ArrayForEach(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

/* malloc/free objects using the malloc/free functions that have been set with cJSON_InitHooks */
void * cJSON_malloc(size_t size);
void cJSON_free(void *object);

#ifdef __cplusplus
}
#endif

#endif /* !_UAPI_LIB_JSON_CORE_H_ */
