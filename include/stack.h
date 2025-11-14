#pragma once

#include <stdlib.h>

#include "varinfo.h"
#include "hashutils.h"

#ifdef STACK_DEBUG
// FIXME varname in the beginning
#define STACK_MAKE(VARNAME)           \
    stk_t VARNAME = {               \
        .buffer   = NULL,             \
        .size     = 0,                \
        .capacity = 0,                \
        .varinfo = {                  \
            .line     = __LINE__,     \
            .filename = __FILE__,     \
            .funcname = __func__,     \
            .varname  = #VARNAME      \
        }                             \
    }

#define IF_DEBUG(statement) statement

#else

#define STACK_MAKE(VARNAME) \
    stk_t VARNAME = {     \
        .buffer   = NULL,   \
        .size     = 0,      \
        .capacity = 0       \
    } 

#define IF_DEBUG(statement) 

#endif // _DEBUG

typedef struct fact_tree_node_t fact_tree_node_t;
typedef const fact_tree_node_t* stack_data_t;

typedef enum stack_err_t
{
    STACK_ERR_NONE,
    STACK_ERR_NULL,
    STACK_ERR_BUFFER_NULL,
    STACK_ERR_SIZE_EXCEED_CAPACITY,
    STACK_ERR_ALLOC_FAIL,
    STACK_ERR_OUT_OF_BOUND,
    STACK_ERR_CANARY_ESCAPED,
    STACK_ERR_HASH_UNMATCH
} stack_err_t;

typedef struct stk_t
{
    IF_DEBUG(
#ifdef CANARY_ENABLED
        stack_data_t canary_begin
#endif // CANARY_ENABLED
    );

    stack_data_t* buffer;
    size_t size;
    size_t capacity;

    IF_DEBUG(
        const varinfo_t varinfo;

#ifdef HASH_ENABLED
        utils_hash_t buffer_hash;
#endif // HASH_ENABLED

#ifdef CANARY_ENABLED
        stack_data_t canary_end;
#endif // CANARY_ENABLED
    );
} stk_t;

stack_err_t stack_ctor(stk_t* stk, size_t capacity);

stack_err_t stack_push(stk_t* stk, stack_data_t val);

stack_err_t stack_pop(stk_t* stk, stack_data_t* val);

const char* stack_strerr(const stack_err_t err);

void stack_dtor(stk_t* stk);

