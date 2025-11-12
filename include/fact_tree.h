#pragma once

#include <stdlib.h>

#include "stringutils.h"

#define FACT_TREE_INIT_LIST \
    {                       \
        .root = NULL,       \
        .size = 0,          \
        .buf = {            \
            .ptr = NULL,    \
            .len = 0,       \
            .pos = 0        \
        }                   \
    };                      

typedef enum fact_tree_err_t
{
    FACT_TREE_ERR_NONE,
    FACT_TREE_INVALID_BUFPOS,
    FACT_TREE_NULLPTR,
    FACT_TREE_ALLOC_FAIL,
    FACT_TREE_IO_ERR,
    FACT_TREE_SYNTAX_ERR
} fact_tree_err_t;

typedef struct fact_tree_node_t
{
    utils_str_t name;
    fact_tree_node_t* left;
    fact_tree_node_t* right;
    fact_tree_node_t* parent;

} fact_tree_node_t;

typedef struct fact_tree_t
{
    fact_tree_node_t* root;
    size_t size;

    struct {
        char* ptr;
        ssize_t len;
        ssize_t pos;
    } buf;

} fact_tree_t;

fact_tree_err_t fact_tree_ctor(fact_tree_t* fact_tree);

void fact_tree_dtor(fact_tree_t* fact_tree);

fact_tree_err_t fact_tree_insert(fact_tree_t* fact_tree, fact_tree_node_t* node, fact_tree_node_t** ret);

fact_tree_node_t* fact_tree_guess(fact_tree_t* fact_tree);

const char* fact_tree_strerr(fact_tree_err_t err);

fact_tree_err_t fact_tree_fwrite(fact_tree_t* fact_tree, const char* filename);

fact_tree_err_t fact_tree_fread(fact_tree_t* fact_tree, const char* filename);

const fact_tree_node_t* fact_tree_find_entity(const fact_tree_node_t* node, const char* name);

char* fact_tree_get_definition(const fact_tree_node_t* node);

void fact_tree_dump(fact_tree_t* fact_tree, fact_tree_err_t err, const char* msg, const char* file, int line, const char* funcname);

#define FACT_TREE_DUMP(fact_tree, err) \
    fact_tree_dump(fact_tree, err, NULL, __FILE__, __LINE__, __func__); 

#define FACT_TREE_DUMP_MSG(fact_tree, err, msg) \
    fact_tree_dump(fact_tree, err, msg, __FILE__, __LINE__, __func__); 
