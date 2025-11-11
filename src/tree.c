#include "fact_tree.h"

#include <cstdlib>
#include <string.h>
#include <time.h>

#include "logutils.h"
#include "memutils.h"
#include "ioutils.h"

#ifdef _DEBUG

#define FACT_TREE_ASSERT_OK_(dllist)                   \
    {                                            \
        fact_tree_err_t err = fact_tree_verify_(fact_tree);        \
        if(err != FACT_TREE_ERR_NONE) {                \
            FACT_TREE_DUMP_(fact_tree, err);                 \
            utils_assert(err == FACT_TREE_ERR_NONE);   \
        }                                        \
    }

#define FACT_TREE_VERIFY_OR_RETURN_(fact_tree, err)    \
    if(err != FACT_TREE_ERR_NONE) {              \
        FACT_TREE_DUMP_(fact_tree, err);               \
        return err;                        \
    }

#else // _DEBUG


#endif // _DEBUG

fact_tree_err_t fact_tree_allocate_new_(fact_tree_node_t** ptr);

void fact_tree_dump_(fact_tree_t* fact_tree, fact_tree_err_t err, const char* msg, const char* file, int line, const char* funcname);

char* fact_tree_dump_graphviz_(fact_tree_t* fact_tree);

int fact_tree_dump_node_graphviz_(FILE* file, fact_tree_node_t* node, int rank);

#ifdef _DEBUG

fact_tree_err_t fact_tree_verify_(fact_tree_t* fact_tree);

void fact_tree_node_dtor_(fact_tree_node_t* node);

#endif // _DEBUG

fact_tree_err_t fact_tree_ctor(fact_tree_t* fact_tree, fact_tree_cmp_f cmp_func)
{
    utils_assert(fact_tree);
    utils_assert(cmp_func);

    fact_tree->cmp_func = cmp_func;
    fact_tree->size = 0;

    return FACT_TREE_ERR_NONE;
}

fact_tree_err_t fact_tree_insert(fact_tree_t* fact_tree, fact_tree_data_t data)
{
    FACT_TREE_ASSERT_OK_(fact_tree);

    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    fact_tree_node_t* parent = fact_tree->root;
    fact_tree_node_t* child = fact_tree->root;

    while(child != NULL) {
        parent = child;

        if(fact_tree->cmp_func(&child->data, &parent->data) >= 0)
            child = parent->left;
        else
            child = parent->right;
    }
    
    fact_tree_node_t* new_elem;
    err = fact_tree_allocate_new_(&new_elem);
    FACT_TREE_VERIFY_OR_RETURN_(fact_tree, err);
    
    new_elem->data = data;
    
    if(!parent) {
        fact_tree->root = new_elem;
    }
    else {
        if(fact_tree->cmp_func(&child->data, &parent->data) >= 0)
            parent->left = new_elem;
        else
            parent->right = new_elem;
    }

    ++fact_tree->size;

    FACT_TREE_DUMP_(fact_tree, err);

    return FACT_TREE_ERR_NONE;
}


const char* fact_tree_strerr(fact_tree_err_t err)
{
    switch(err) {
        case FACT_TREE_ERR_NONE:
            return "none";
        case FACT_TREE_NULLPTR:
            return "passed a nullptr";
        case FACT_TREE_ALLOC_FAIL:
            return "memory allocation failed";
        default:
            return "unknown";
    }
}

void fact_tree_dtor(fact_tree_t* fact_tree)
{
    utils_assert(fact_tree);

    fact_tree_node_dtor_(fact_tree->root); 
}

fact_tree_err_t fact_tree_allocate_new_(fact_tree_node_t** ptr)
{
    utils_assert(ptr);

    fact_tree_node_t* ptr_tmp = (fact_tree_node_t*)calloc(1, sizeof(ptr_tmp[0]));

    if(!ptr_tmp)
        return FACT_TREE_ALLOC_FAIL;

    *ptr = ptr_tmp;

    return FACT_TREE_ERR_NONE;
}

void fact_tree_node_dtor_(fact_tree_node_t* node)
{
    utils_assert(node);

    if(node->left)
        fact_tree_node_dtor_(node->left);
    if(node->right)
        fact_tree_node_dtor_(node->right);

    NFREE(node);
}

#ifdef _DEBUG

#define GRAPHVIZ_FNAME_ "graphviz"
#define GRAPHVIZ_CMD_LEN_ 100

#define CLR_RED_LIGHT_   "\"#FFB0B0\""
#define CLR_GREEN_LIGHT_ "\"#B0FFB0\""
#define CLR_BLUE_LIGHT_  "\"#B0B0FF\""

#define CLR_RED_BOLD_    "\"#FF0000\""
#define CLR_GREEN_BOLD_  "\"#03c03c\""
#define CLR_BLUE_BOLD_   "\"#0000FF\""

void fact_tree_dump_(fact_tree_t* fact_tree, fact_tree_err_t err, const char* msg, const char* filename, int line, const char* funcname)
{
    utils_log_fprintf(
        "<style>"
        "table {"
          "border-collapse: collapse;"
          "border: 1px solid;"
          "font-size: 0.9em;"
        "}"
        "th,"
        "td {"
          "border: 1px solid rgb(160 160 160);"
          "padding: 8px 10px;"
        "}"
        "</style>\n"
    );

    utils_log_fprintf("<pre>\n"); 

    time_t cur_time = time(NULL);
    struct tm* iso_time = localtime(&cur_time);
    char time_buff[100];
    strftime(time_buff, sizeof(time_buff), "%F %T", iso_time);

    if(err != FACT_TREE_ERR_NONE) {
        utils_log_fprintf("<h3 style=\"color:red;\">[ERROR] [%s] from %s:%d: %s() </h3>", time_buff, filename, line, funcname);
        utils_log_fprintf("<h4><font color=\"red\">err: %s </font></h4>", fact_tree_strerr(err));
    }
    else
        utils_log_fprintf("<h3>[DEBUG] [%s] from %s:%d: %s() </h3>\n", time_buff, filename, line, funcname);

    if(msg)
        utils_log_fprintf("what: %s\n", msg);

    char* img_pref = fact_tree_dump_graphviz_(fact_tree);

    utils_log_fprintf(
        "\n<img src=" IMG_DIR "/%s.svg width=50%%\n", 
        strrchr(img_pref, '/') + 1
    );

    utils_log_fprintf("</pre>\n"); 

    utils_log_fprintf("<hr color=\"black\" />\n");

    NFREE(img_pref);
}

char* fact_tree_dump_graphviz_(fact_tree_t* fact_tree)
{

    FILE* file = open_file(LOG_DIR "/" GRAPHVIZ_FNAME_ ".txt", "w");
    if(!file)
        exit(EXIT_FAILURE);

    fprintf(file, "digraph {\n rankdir=TB;\n"); 
    fprintf(file, "nodesep=0.9;\nranksep=0.75;\n");

    fact_tree_dump_node_graphviz_(file, fact_tree->root, 1);

    fprintf(file, "};");

    fclose(file);
    
    create_dir(LOG_DIR "/" IMG_DIR);
    char* img_tmpnam = tempnam(LOG_DIR "/" IMG_DIR, "img-");
    utils_assert(img_tmpnam);

    static char strbuf[GRAPHVIZ_CMD_LEN_]= "";

    sprintf(
        strbuf, 
        "dot -T svg -o %s.svg " LOG_DIR "/" GRAPHVIZ_FNAME_ ".txt", 
        img_tmpnam
    );

    system(strbuf);

    return img_tmpnam;
}

int fact_tree_dump_node_graphviz_(FILE* file, fact_tree_node_t* node, int rank)
{
    utils_assert(file);
    utils_assert(node);

    static int node_cnt = 0;

    if(rank == 1)
        node_cnt = 0;

    int node_cnt_left = 0, node_cnt_right = 0;
    
    if(node->left)
        node_cnt_left = fact_tree_dump_node_graphviz_(file, node->left, rank + 1);
    if(node->right)
        node_cnt_right = fact_tree_dump_node_graphviz_(file, node->right, rank + 1);
    
    fprintf(
        file, 
        "node_%d["
        "shape=record,"
        "label=\" { addr: %p | data: %d | { L: %p | R: %p } } \","
        "rank=%d"
        "];\n",
        node_cnt,
        node,
        node->data,
        node->left,
        node->right,
        rank
    );

    if(node->left)
        fprintf(
            file,
            "node_%d -> node_%d;\n",
            node_cnt, 
            node_cnt_left
        );

    if(node->right)
        fprintf(
            file,
            "node_%d -> node_%d;\n",
            node_cnt, 
            node_cnt_right
        );

    return node_cnt++;
}

fact_tree_err_t fact_tree_verify_(fact_tree_t* fact_tree)
{
    if(!fact_tree)
        return FACT_TREE_NULLPTR;

    if(!fact_tree->cmp_func)
        return FACT_TREE_FIELD_NULL;

    
    return FACT_TREE_ERR_NONE;
}


#endif // _DEBUG
