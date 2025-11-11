#include "fact_tree.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "colorutils.h"
#include "logutils.h"
#include "memutils.h"
#include "ioutils.h"
#include "sortutils.h"
#include "stringutils.h"
#include "assertutils.h"

#define DEFAULT_NODE_ "nothing"
#define CHAR_ACCEPT_ 'y'
#define CHAR_DECLINE_ 'n'

#ifdef _DEBUG

#define FACT_TREE_ASSERT_OK_(fact_tree)                      \
    {                                                        \
        fact_tree_err_t err = fact_tree_verify_(fact_tree);  \
        if(err != FACT_TREE_ERR_NONE) {                      \
            FACT_TREE_DUMP(fact_tree, err);                  \
            utils_assert(err == FACT_TREE_ERR_NONE);         \
        }                                                    \
    }

#else // _DEBUG


#endif // _DEBUG

fact_tree_err_t fact_tree_allocate_new_node_(fact_tree_node_t** node, utils_str_t title);

void fact_tree_swap_nodes_(fact_tree_node_t* node_a, fact_tree_node_t* node_b);

fact_tree_err_t fact_tree_fwrite_node_(fact_tree_node_t* node, FILE* file);

char* fact_tree_dump_graphviz_(fact_tree_t* fact_tree);

int fact_tree_dump_node_graphviz_(FILE* file, fact_tree_node_t* node, int rank);

#ifdef _DEBUG

fact_tree_err_t fact_tree_verify_(fact_tree_t* fact_tree);

void fact_tree_node_dtor_(fact_tree_node_t* node);

#endif // _DEBUG

fact_tree_err_t fact_tree_ctor(fact_tree_t* fact_tree)
{
    utils_assert(fact_tree);

    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    utils_str_t deflt_s = {
        .str = strdup(DEFAULT_NODE_),
        .len = SIZEOF(DEFAULT_NODE_) - 1
    };
    
    fact_tree_allocate_new_node_(&fact_tree->root, deflt_s);

    fact_tree->size = 1;

    FACT_TREE_DUMP(fact_tree, err);

    return FACT_TREE_ERR_NONE;
}

void fact_tree_dtor(fact_tree_t* fact_tree)
{
    utils_assert(fact_tree);

    fact_tree_node_dtor_(fact_tree->root); 
}

fact_tree_err_t fact_tree_insert(fact_tree_t* fact_tree)
{
    FACT_TREE_ASSERT_OK_(fact_tree);

    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    fact_tree_node_t* node = fact_tree->root;

    while(node->right != NULL) {
        utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, "Entity ..."                  );
        utils_colored_fprintf(stdout, ANSI_COLOR_CYAN,       "%s",          node->title.str);
        utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, "? [y/N]: "                   );

        char input = CHAR_DECLINE_;
        scanf("%c", &input);
        clear_stdin_buffer();

        if(input == CHAR_ACCEPT_)
            node = node->right;
        else
            node = node->left;
    }

    utils_str_t diff_s = UTILS_STR_INITLIST;
    utils_str_t entity_s = UTILS_STR_INITLIST;
    enum io_err_t io_err = IO_ERR_NONE;

    utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, "Enter entity's name: "); 
    io_err = input_string_until_correct(&entity_s.str, &entity_s.len);
    io_err == IO_ERR_NONE verified(return FACT_TREE_IO_ERR);

    utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, "Enter the difference between "                );
    utils_colored_fprintf(stdout, ANSI_COLOR_MAGENTA,    "%s",                           entity_s.str   );
    utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, " and "                                        );
    utils_colored_fprintf(stdout, ANSI_COLOR_MAGENTA,    "%s",                           node->title.str);
    utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, ": "                                           );

    io_err = input_string_until_correct(&diff_s.str, &diff_s.len);
    io_err == IO_ERR_NONE verified(return FACT_TREE_IO_ERR);

    fact_tree_node_t *node_entity_old, *node_entity_new;

    err = fact_tree_allocate_new_node_(&node_entity_old, diff_s);
    err == FACT_TREE_ERR_NONE verified(return FACT_TREE_ALLOC_FAIL);

    fact_tree_swap_nodes_(node_entity_old, node);

    err = fact_tree_allocate_new_node_(&node_entity_new, entity_s);
    err == FACT_TREE_ERR_NONE verified(return FACT_TREE_ALLOC_FAIL);

    node->left = node_entity_old;
    node->right = node_entity_new;

    ++fact_tree->size;

    FACT_TREE_DUMP(fact_tree, err);

    return FACT_TREE_ERR_NONE;
}


fact_tree_err_t fact_tree_fwrite(fact_tree_t* fact_tree, const char* filename)
{
    utils_assert(filename);

    FILE* file = open_file(filename, "w");
    file verified(return FACT_TREE_IO_ERR);

    fact_tree_fwrite_node_(fact_tree->root, file);
}

fact_tree_err_t fact_tree_fwrite_node_(fact_tree_node_t* node, FILE* file)
{
    utils_assert(node);
    utils_assert(file);

    fputc('(', file);

    if(node->left)
        fact_tree_fwrite_node_(node->left, file);

    fprintf(file, " %s ", node->title.str);

    if(node->right)
        fact_tree_fwrite_node_(node->right, file);

    fputc(')', file);

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

void fact_tree_node_dtor_(fact_tree_node_t* node)
{
    utils_assert(node);

    if(node->left)
        fact_tree_node_dtor_(node->left);
    if(node->right)
        fact_tree_node_dtor_(node->right);

    NFREE(node->title.str);
    NFREE(node);
}

fact_tree_err_t fact_tree_allocate_new_node_(fact_tree_node_t** node, utils_str_t title)
{
    utils_assert(node);

    fact_tree_node_t* node_tmp = (fact_tree_node_t*)calloc(1, sizeof(node_tmp[0]));
    
    node_tmp verified(return FACT_TREE_ALLOC_FAIL);

    node_tmp->title = title;

    *node = node_tmp;

    return FACT_TREE_ERR_NONE;
}

void fact_tree_swap_nodes_(fact_tree_node_t* node_a, fact_tree_node_t* node_b)
{
    utils_swap(&node_a->title, &node_b->title, sizeof(node_a->title));
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

void fact_tree_dump(fact_tree_t* fact_tree, fact_tree_err_t err, const char* msg, const char* filename, int line, const char* funcname)
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
        "label=\" { addr: %p | title: \' %s \' | { L: %p | R: %p } } \","
        "rank=%d"
        "];\n",
        node_cnt,
        node,
        node->title.str,
        node->left,
        node->right,
        rank
    );

    if(node->left)
        fprintf(
            file,
            "node_%d -> node_%d [label=\"No\"];\n",
            node_cnt, 
            node_cnt_left
        );

    if(node->right)
        fprintf(
            file,
            "node_%d -> node_%d [label=\"Yes\"];\n",
            node_cnt, 
            node_cnt_right
        );

    return node_cnt++;
}

fact_tree_err_t fact_tree_verify_(fact_tree_t* fact_tree)
{
    if(!fact_tree)
        return FACT_TREE_NULLPTR;

    
    return FACT_TREE_ERR_NONE;
}


#endif // _DEBUG
