#include "fact_tree.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#include <festival/festival.h>

#include "imgui.h"

#include "colorutils.h"
#include "logutils.h"
#include "memutils.h"
#include "ioutils.h"
#include "sortutils.h"
#include "stringutils.h"
#include "assertutils.h"
#include "logutils.h"
#include "stack.h"

#define LOG_CATEGORY_FTREE "FACT TREE"

#define DEFAULT_NODE_ "nothing"
#define CHAR_ACCEPT_ 'y'
#define CHAR_DECLINE_ 'n'
#define NIL_STR "nil"

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

#define FACT_TREE_ASSERT_OK_(fact_tree) 

#endif // _DEBUG

static fact_tree_err_t fact_tree_allocate_new_node_(fact_tree_node_t** node, utils_str_t title);

static void fact_tree_swap_nodes_(fact_tree_node_t* node_a, fact_tree_node_t* node_b);

static fact_tree_err_t fact_tree_fwrite_node_(fact_tree_node_t* node, FILE* file);

static fact_tree_err_t fact_tree_fread_node_(fact_tree_t* ftree, fact_tree_node_t** node, const char* fname);

static fact_tree_err_t fact_tree_scan_node_name_(fact_tree_t* ftree);

static int fact_tree_advance_buf_pos_(fact_tree_t* ftree);

static void fact_tree_skip_spaces_(fact_tree_t* ftree);

static int fact_tree_get_height(fact_tree_t* tree);

static void fact_tree_print_node_definition_(const fact_tree_node_t* node, const char* end);

static char* fact_tree_dump_graphviz_(fact_tree_t* fact_tree);

static void fact_tree_dump_node_graphviz_(FILE* file, fact_tree_node_t* node, int rank);

#ifdef _DEBUG

fact_tree_err_t fact_tree_verify_(fact_tree_t* fact_tree);

void fact_tree_node_dtor_(fact_tree_t* tree, fact_tree_node_t* node);

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

    fact_tree_node_dtor_(fact_tree, fact_tree->root); 

    fact_tree->size = 0;
    fact_tree->root = NULL;

    NFREE(fact_tree->buf.ptr);
    fact_tree->buf.pos = 0;
    fact_tree->buf.len = 0;
}

void fact_tree_node_dtor_(fact_tree_t* ftree, fact_tree_node_t* node)
{
    if(!node) return;

    if(node->left)
        fact_tree_node_dtor_(ftree, node->left);
    if(node->right)
        fact_tree_node_dtor_(ftree, node->right);

    if(ftree->buf.ptr) {
        if(node->name.str >= ftree->buf.ptr + ftree->buf.len 
                || node->name.str < ftree->buf.ptr)
            NFREE(node->name.str);
    }
    else
        NFREE(node->name.str);

    NFREE(node);
}

fact_tree_node_t* fact_tree_guess(fact_tree_t* fact_tree)
{
    FACT_TREE_ASSERT_OK_(fact_tree);
    
    if(!fact_tree->root) {
        UTILS_LOGE(LOG_CATEGORY_FTREE, "tree is not initialized");
        return NULL;
    }

    fact_tree_node_t* node = fact_tree->root;

    // char input = CHAR_DECLINE_;
    while(node->right != NULL) {
        // utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, "Is object ... "              );
        // utils_colored_fprintf(stdout, ANSI_COLOR_CYAN,       "%s",           node->name.str);
        // utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, "? [y/N]: "                   );
        //
        // scanf("%c", &input);
        // clear_stdin_buffer();
        
        ImGui::Text("Is object %s ...?", node->name.str);

        if(ImGui::Button("Yes"))
            node = node->right;
        else if(ImGui::Button("No"))
            node = node->left;
    }
    
    return node;
}

fact_tree_err_t fact_tree_insert(fact_tree_t* fact_tree, fact_tree_node_t* node, fact_tree_node_t** ret)
{
    FACT_TREE_ASSERT_OK_(fact_tree);
    utils_assert(ret);
    utils_assert(node);

    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    utils_str_t diff_s = UTILS_STR_INITLIST;
    utils_str_t entity_s = UTILS_STR_INITLIST;
    enum io_err_t io_err = IO_ERR_NONE;

    utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, "Enter object's name: "); 
    io_err = input_string_until_correct(&entity_s.str, &entity_s.len);
    io_err == IO_ERR_NONE verified(return FACT_TREE_IO_ERR);

    utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, "Enter the difference between "                );
    utils_colored_fprintf(stdout, ANSI_COLOR_MAGENTA,    "%s",                           entity_s.str   );
    utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, " and "                                        );
    utils_colored_fprintf(stdout, ANSI_COLOR_MAGENTA,    "%s",                           node->name.str);
    utils_colored_fprintf(stdout, ANSI_COLOR_BOLD_WHITE, ": "                                           );

    io_err = input_string_until_correct(&diff_s.str, &diff_s.len);
    io_err == IO_ERR_NONE verified(return FACT_TREE_IO_ERR);

    fact_tree_node_t *node_entity_old = NULL, *node_entity_new = NULL;

    err = fact_tree_allocate_new_node_(&node_entity_old, diff_s);
    err == FACT_TREE_ERR_NONE verified(return FACT_TREE_ALLOC_FAIL);

    fact_tree_swap_nodes_(node_entity_old, node);

    err = fact_tree_allocate_new_node_(&node_entity_new, entity_s);
    err == FACT_TREE_ERR_NONE verified(return FACT_TREE_ALLOC_FAIL);

    node->left = node_entity_old;
    node->right = node_entity_new;
    
    node_entity_new->parent = node;
    node_entity_old->parent = node;

    ++fact_tree->size;

    *ret = node_entity_new;

    return FACT_TREE_ERR_NONE;
}

const fact_tree_node_t* fact_tree_find_object(const fact_tree_node_t* node, const char* name)
{
    const fact_tree_node_t *cur = NULL, *ret = NULL;

    if(node->left && (ret = fact_tree_find_object(node->left, name)))
        cur = ret;
    
    if(node->right && (ret = fact_tree_find_object(node->right, name)))
        cur = ret;

    if(!node->left && !node->right) {
        if(strncmp(node->name.str, name, node->name.len) == 0)
            cur = node;
    }
    
    return cur;
}

fact_tree_err_t fact_tree_get_object_path(fact_tree_t* ftree, const fact_tree_node_t* node, stk_t* stk)
{
    utils_assert(node);
    utils_assert(stk);

    stack_ctor(stk, (size_t) fact_tree_get_height(ftree));

    while(node) {
        stack_push(stk, node);
        node = node->parent;
    }

    return FACT_TREE_ERR_NONE;
}

void fact_tree_print_node_definition_(const fact_tree_node_t* node, const char* end)
{
    utils_assert(node);
    utils_assert(node->parent);
    utils_assert(end);

    if(node == node->parent->left)
        printf_and_say(" not %s%s", node->parent->name.str, end);
    else if(node == node->parent->right)
        printf_and_say(" %s%s", node->parent->name.str, end);
}

fact_tree_err_t fact_tree_print_definition(fact_tree_t* ftree, const fact_tree_node_t* node)
{
    FACT_TREE_ASSERT_OK_(ftree);
    utils_assert(node);

    fact_tree_err_t tree_err = FACT_TREE_ERR_NONE;
    
    STACK_MAKE(stk);

    tree_err = fact_tree_get_object_path(ftree, node, &stk);
    tree_err == FACT_TREE_ERR_NONE verified(return tree_err);

    printf_and_say("%s", node->name.str);

    const fact_tree_node_t *cur = NULL;
    stack_pop(&stk, &cur);

    while(stk.size) {
        stack_pop(&stk, &cur);
        fact_tree_print_node_definition_(cur, stk.size ? "," : "");
    }
    
    putc('\n', stdout);

    return tree_err;
}

fact_tree_err_t fact_tree_print_difference(fact_tree_t* ftree, const fact_tree_node_t* node_a, const fact_tree_node_t* node_b)
{
    FACT_TREE_ASSERT_OK_(ftree);
    utils_assert(node_a);
    utils_assert(node_b);

    fact_tree_err_t tree_err = FACT_TREE_ERR_NONE;
    
    STACK_MAKE(stk_a); STACK_MAKE(stk_b);

    tree_err = fact_tree_get_object_path(ftree, node_a, &stk_a);
    tree_err == FACT_TREE_ERR_NONE verified(return tree_err);

    tree_err = fact_tree_get_object_path(ftree, node_b, &stk_b);
    tree_err == FACT_TREE_ERR_NONE verified(return tree_err);

    const fact_tree_node_t *cur_a = NULL, *cur_b = NULL;

    stack_pop(&stk_a, &cur_a); stack_pop(&stk_a, &cur_a);
    stack_pop(&stk_b, &cur_b); stack_pop(&stk_b, &cur_b);

    printf_and_say("%s and %s both:", node_a->name.str, node_b->name.str);

    while(cur_a == cur_b) {
        fact_tree_print_node_definition_(cur_a, "");
        stack_pop(&stk_a, &cur_a);
        stack_pop(&stk_b, &cur_b);
        if(cur_a == cur_b) printf(",");
    }

    printf_and_say(", but %s", node_a->name.str);

    for( ;; ) {
        fact_tree_print_node_definition_(cur_a, stk_a.size ? "," : "");
        if(stk_a.size) stack_pop(&stk_a, &cur_a);
        else break;
    }

    printf_and_say(", and %s", node_b->name.str);

    for( ;; ) {
        fact_tree_print_node_definition_(cur_b, stk_b.size ? "," : "");
        if(stk_b.size) stack_pop(&stk_b, &cur_b);
        else break;
    }
    
    puts(".\n");

    return tree_err;
}

#undef BUF_INITIAL_SIZE_

fact_tree_err_t fact_tree_fwrite(fact_tree_t* fact_tree, const char* filename)
{
    utils_assert(filename);

    FILE* file = open_file(filename, "w");
    file verified(return FACT_TREE_IO_ERR);

    fact_tree_err_t err = fact_tree_fwrite_node_(fact_tree->root, file);

    UTILS_LOGD(LOG_CATEGORY_FTREE, "Writing done");

    fclose(file);

    return err;
}

fact_tree_err_t fact_tree_fwrite_node_(fact_tree_node_t* node, FILE* file)
{
    utils_assert(node);
    utils_assert(file);

    fact_tree_err_t err = FACT_TREE_ERR_NONE;
    int io_err = 0;

    io_err = fprintf(file, "(");
    io_err >= 0 verified(return FACT_TREE_IO_ERR);

    io_err = fprintf(file, " \"%s\" ", node->name.str);
    io_err >= 0 verified(return FACT_TREE_IO_ERR);

    if(node->left)
        err = fact_tree_fwrite_node_(node->left, file);
    else
        fprintf(file, NIL_STR);

    if(node->right)
        err = fact_tree_fwrite_node_(node->right, file);
    else
        fprintf(file, " " NIL_STR " ");

    io_err = fprintf(file, ")");
    io_err >= 0 verified(return FACT_TREE_IO_ERR);

    return err;
}

int fact_tree_advance_buf_pos_(fact_tree_t* ftree)
{
    FACT_TREE_ASSERT_OK_(ftree);

    if(ftree->buf.pos < ftree->buf.len - 1) {
        ftree->buf.pos++;
        return 0;
    }
    else
        return 1;
}

void fact_tree_skip_spaces_(fact_tree_t* ftree)
{
    FACT_TREE_ASSERT_OK_(ftree);

    while(isspace(ftree->buf.ptr[ftree->buf.pos])) {
        if(fact_tree_advance_buf_pos_(ftree))
            break;
    }
}

static int fact_tree_get_height(fact_tree_t* ftree)
{
    FACT_TREE_ASSERT_OK_(ftree);

    return (int)ceil(log2(ftree->size));
}

fact_tree_err_t fact_tree_scan_node_name_(fact_tree_t* ftree)
{
    FACT_TREE_ASSERT_OK_(ftree);

    int name_len = 0;
    sscanf(ftree->buf.ptr + ftree->buf.pos, " \"%*[^\"]\"%n", &name_len);

    ftree->buf.pos += (size_t) name_len;

    ftree->buf.ptr[ftree->buf.pos - 1] = '\0';

    return FACT_TREE_ERR_NONE;
}

#define FTREE_LOG_SYNTAX_ERR(fname, ftree, expc) \
    UTILS_LOGE( \
        LOG_CATEGORY_FTREE, \
        "%s:1:%ld: syntax error: unexpected symbol <%c>, expected <" expc ">", \
        fname, \
        ftree->buf.pos, \
        ftree->buf.ptr[ftree->buf.pos] \
    );

fact_tree_err_t fact_tree_fread_node_(fact_tree_t* ftree, fact_tree_node_t** node, const char* fname)
{
    FACT_TREE_ASSERT_OK_(ftree);

    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    FACT_TREE_DUMP(ftree, err);

    if(ftree->buf.ptr[ftree->buf.pos] == '(') {
        utils_str_t name = { .str = NULL, .len = 0 };
        err = fact_tree_allocate_new_node_(node, name);
        err == FACT_TREE_ERR_NONE verified(return err);

        fact_tree_advance_buf_pos_(ftree);
        fact_tree_skip_spaces_(ftree);

        if(ftree->buf.ptr[ftree->buf.pos] != '"') {
            FTREE_LOG_SYNTAX_ERR(fname, ftree, "\"");
            return FACT_TREE_SYNTAX_ERR;
        }

        ssize_t buf_pos_prev = ftree->buf.pos;
        err = fact_tree_scan_node_name_(ftree);
        err == FACT_TREE_ERR_NONE verified(return err);

        (*node)->name.str = ftree->buf.ptr + buf_pos_prev + 1;
        (*node)->name.len = (size_t)(ftree->buf.pos - buf_pos_prev);

        fact_tree_skip_spaces_(ftree);

        err = fact_tree_fread_node_(ftree, &(*node)->left, fname);
        err == FACT_TREE_ERR_NONE verified(return err);

        if((*node)->left) {
            UTILS_LOGD(LOG_CATEGORY_FTREE, "%s -> %s", (*node)->left->name.str, (*node)->name.str);
            (*node)->left->parent = (*node);
        }

        fact_tree_skip_spaces_(ftree);

        err = fact_tree_fread_node_(ftree, &(*node)->right, fname);
        err == FACT_TREE_ERR_NONE verified(return err);

        if((*node)->right) {
            UTILS_LOGD(LOG_CATEGORY_FTREE, "%s -> %s", (*node)->right->name.str, (*node)->name.str);
            (*node)->right->parent = (*node);
        }

        ftree->size++;

        if(ftree->buf.ptr[ftree->buf.pos] != ')') {
            FTREE_LOG_SYNTAX_ERR(fname, ftree, ")");
            return FACT_TREE_SYNTAX_ERR;
        }

        fact_tree_advance_buf_pos_(ftree);
        fact_tree_skip_spaces_(ftree);
    }
    else if(strncmp(ftree->buf.ptr + ftree->buf.pos, NIL_STR, SIZEOF(NIL_STR) - 1) == 0) {

        if(ftree->buf.ptr[ftree->buf.pos] != 'n') {
            FTREE_LOG_SYNTAX_ERR(fname, ftree, "n");
            return FACT_TREE_SYNTAX_ERR;
        }

        ftree->buf.pos += SIZEOF(NIL_STR) - 1;
        fact_tree_skip_spaces_(ftree);
        *node = NULL;
    }
    else {
        FTREE_LOG_SYNTAX_ERR(fname, ftree, "(");
        return FACT_TREE_SYNTAX_ERR;
    }

    return FACT_TREE_ERR_NONE;
}

#undef FTREE_LOG_SYNTAX_ERR

fact_tree_err_t fact_tree_fread(fact_tree_t* ftree, const char* filename)
{
    utils_assert(ftree);
    utils_assert(filename);

    fact_tree_dtor(ftree);

    FILE* file = open_file(filename, "r");
    file verified(return FACT_TREE_IO_ERR);
    
    size_t fsize = get_file_size(file);

    ftree->buf.ptr = TYPED_CALLOC(fsize, char);
    ftree->buf.ptr verified(return FACT_TREE_ALLOC_FAIL);

    size_t bytes_transferred = fread(ftree->buf.ptr, sizeof(ftree->buf.ptr[0]), fsize, file);
    fclose(file);
    // TODO check for errors
    ftree->buf.len = (unsigned) bytes_transferred;
    
    fact_tree_err_t err = fact_tree_fread_node_(ftree, &ftree->root, filename);

    if(err != FACT_TREE_ERR_NONE) {
        FACT_TREE_DUMP(ftree, err);
        return err;
    }

    FACT_TREE_DUMP(ftree, err);

    return FACT_TREE_ERR_NONE;
}

const char* fact_tree_strerr(fact_tree_err_t err)
{
    switch(err) {
        case FACT_TREE_ERR_NONE:
            return "none";
        case FACT_TREE_NULLPTR:
            return "passed a nullptr";
        case FACT_TREE_INVALID_BUFPOS:
            return "buffer position invalid";
        case FACT_TREE_ALLOC_FAIL:
            return "memory allocation failed";
        case FACT_TREE_IO_ERR:
            return "io error";
        case FACT_TREE_SYNTAX_ERR:
            return "syntax error";
        default:
            return "unknown";
    }
}

fact_tree_err_t fact_tree_allocate_new_node_(fact_tree_node_t** node, utils_str_t name)
{
    utils_assert(node);

    fact_tree_node_t* node_tmp = (fact_tree_node_t*)calloc(1, sizeof(node_tmp[0]));
    
    node_tmp verified(return FACT_TREE_ALLOC_FAIL);

    node_tmp->name = name;

    *node = node_tmp;

    return FACT_TREE_ERR_NONE;
}

void fact_tree_swap_nodes_(fact_tree_node_t* node_a, fact_tree_node_t* node_b)
{
    // FIXME fix swap
    utils_swap(&node_a->name, &node_b->name, sizeof(node_a->name));
}

void printf_and_say(const char* fmt, ...)
{
    utils_assert(fmt);

    const size_t buffer_size = 100;
    char buffer[buffer_size] = "";

    va_list va_arg_list;
    va_start(va_arg_list, fmt);
    vsnprintf(buffer, buffer_size, fmt, va_arg_list);
    va_end(va_arg_list);

    printf("%s", buffer);
    fflush(stdout);

    festival_say_text(buffer);
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


    BEGIN {
        if(!fact_tree->buf.ptr) GOTO_END;

        utils_log_fprintf("buf.pos = %ld\n", fact_tree->buf.pos); 
        utils_log_fprintf("buf.len = %ld\n", fact_tree->buf.len); 
        utils_log_fprintf("buf.ptr[%p] = ", fact_tree->buf.ptr); 

        if(err == FACT_TREE_INVALID_BUFPOS) {
            for(ssize_t i = 0; i < fact_tree->buf.len; ++i) {
                utils_log_fprintf("%c", fact_tree->buf.ptr[i]);
            }    
            utils_log_fprintf("\n");
            GOTO_END;
        }

        utils_log_fprintf("<font color=\"#09AB00\">"); 
        for(ssize_t i = 0; i < fact_tree->buf.pos; ++i) {
            utils_log_fprintf("%c", fact_tree->buf.ptr[i]);
        }
        utils_log_fprintf("</font>");


        utils_log_fprintf("<font color=\"#C71022\"><b>%c</b></font>", fact_tree->buf.ptr[fact_tree->buf.pos]);


        utils_log_fprintf("<font color=\"#1022C7\">"); 
        for(ssize_t i = fact_tree->buf.pos + 1; i < fact_tree->buf.len; ++i) {
            utils_log_fprintf("%c", fact_tree->buf.ptr[i]);
        }
        utils_log_fprintf("</font>\n");

    } END;

    char* img_pref = fact_tree_dump_graphviz_(fact_tree);

    utils_log_fprintf(
        "\n<img src=" IMG_DIR "/%s.svg width=70%%\n", 
        strrchr(img_pref, '/') + 1
    );

    utils_log_fprintf("</pre>\n"); 

    utils_log_fprintf("<hr color=\"black\" />\n");

    NFREE(img_pref);
}

char* fact_tree_dump_graphviz_(fact_tree_t* fact_tree)
{
    utils_assert(fact_tree);

    FILE* file = open_file(LOG_DIR "/" GRAPHVIZ_FNAME_ ".txt", "w");
    // FIXME exit
    if(!file)
        exit(EXIT_FAILURE);

    fprintf(file, "digraph {\n rankdir=TB;\n"); 
    fprintf(file, "nodesep=0.9;\nranksep=0.75;\n");

    // fprintf(
    //     file, 
    //     "node [fontname=\"Fira Mono\","
    //     "color=" CLR_RED_BOLD_","
    //     "style=\"filled\","
    //     "shape=tripleoctagon,"
    //     "fillcolor=" CLR_RED_LIGHT_ ","
    //     "];\n"
    // );

    fact_tree_dump_node_graphviz_(file, fact_tree->root, 1);

    fprintf(file, "};");

    fclose(file);
    
    create_dir(LOG_DIR "/" IMG_DIR);
    char* img_tmpnam = tempnam(LOG_DIR "/" IMG_DIR, "img-");
    utils_assert(img_tmpnam);

    static char strbuf[GRAPHVIZ_CMD_LEN_]= "";

    // snprintf
    sprintf(
        strbuf, 
        "dot -T svg -o %s.svg " LOG_DIR "/" GRAPHVIZ_FNAME_ ".txt", 
        img_tmpnam
    );

    system(strbuf);

    return img_tmpnam;
}

void fact_tree_dump_node_graphviz_(FILE* file, fact_tree_node_t* node, int rank)
{
    utils_assert(file);

    if(!node) return;

    if(node->left)
        fact_tree_dump_node_graphviz_(file, node->left, rank + 1); 
    if(node->right) 
        fact_tree_dump_node_graphviz_(file, node->right, rank + 1);

    if(!node->left && !node->right)
        fprintf(
            file, 
            "node_%p["
            "shape=record,"
            "label=\" { parent: %p | addr: %p | name: \' %s \' | { L: %p | R: %p } } \","
            "style=\"filled\","
            "color=" CLR_GREEN_BOLD_ ","
            "fillcolor=" CLR_GREEN_LIGHT_ ","
            "rank=%d"
            "];\n",
            node,
            node->parent,
            node,
            node->name.str,
            node->left,
            node->right,
            rank
        );
    else
        fprintf(
            file, 
            "node_%p["
            "shape=none,"
            "label=<"
            "<table cellspacing=\"0\" border=\"0\" cellborder=\"1\">"
              "<tr>"
                "<td colspan=\"2\">parent %p</td>"
              "</tr>"
              "<tr>"
                "<td colspan=\"2\">addr: %p</td>"
              "</tr>"
              "<tr>"
                "<td colspan=\"2\">name: %s</td>"
              "</tr>"
              "<tr>"
                "<td bgcolor=" CLR_RED_LIGHT_ ">L: %p</td>"
                "<td bgcolor=" CLR_BLUE_LIGHT_">R: %p</td>"
              "</tr>"
            "</table>>,"
            "rank=%d,"
            "];\n",
            node,
            node->parent,
            node,
            node->name.str,
            node->left,
            node->right,
            rank
        );

    if(node->left)
        fprintf(
            file,
            "node_%p -> node_%p ["
            "dir=both," 
            "label=\"No\","
            "color=" CLR_RED_BOLD_ ","
            "fontcolor=" CLR_RED_BOLD_ ","
            "];\n",
            node, 
            node->left
        );

    if(node->right)
        fprintf(
            file,
            "node_%p -> node_%p ["
            "dir=both," 
            "label=\"Yes\","
            "color=" CLR_BLUE_BOLD_ ","
            "fontcolor=" CLR_BLUE_BOLD_ ","
            "];\n",
            node, 
            node->right
        );
}

fact_tree_err_t fact_tree_verify_(fact_tree_t* fact_tree)
{
    if(!fact_tree)
        return FACT_TREE_NULLPTR;

    if(fact_tree->buf.pos > fact_tree->buf.len)
        return FACT_TREE_INVALID_BUFPOS;
    
    return FACT_TREE_ERR_NONE;
}


#endif // _DEBUG
