#pragma once

#include <stdlib.h>

#define FACT_TREE_DUMP(fact_tree, err) \
    fact_tree_dump(fact_tree, err, NULL, __FILE__, __LINE__, __func__); 

#define FACT_TREE_DUMP_MSG(fact_tree, err, msg) \
    fact_tree_dump(fact_tree, err, msg, __FILE__, __LINE__, __func__); 

typedef enum fact_tree_err_t
{
    FACT_TREE_ERR_NONE,
    FACT_TREE_NULLPTR,
    FACT_TREE_FIELD_NULL,
    FACT_TREE_ALLOC_FAIL
} fact_tree_err_t;

typedef struct fact_tree_node_t
{
    char* str;
    fact_tree_node_t* left;
    fact_tree_node_t* right;
} fact_tree_node_t;

typedef struct fact_tree_t
{
    fact_tree_node_t* root;
    size_t size;
} fact_tree_t;

fact_tree_err_t fact_tree_ctor(fact_tree_t* fact_tree);

fact_tree_err_t fact_tree_insert(fact_tree_t* fact_tree, char* str);

const char* fact_tree_strerr(fact_tree_err_t err);

void fact_tree_dtor(fact_tree_t* fact_tree);

void fact_tree_dump(fact_tree_t* fact_tree, fact_tree_err_t err, const char* msg, const char* file, int line, const char* funcname);

