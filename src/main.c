#include "fact_tree.h"

#include "optutils.h"
#include "utils.h"
#include "logutils.h"

static utils_long_opt_t long_opts[] = 
{
    { OPT_ARG_REQUIRED, "log", NULL, 0, 0 },
};

int main(int argc, char* argv[])
{
    utils_long_opt_get(argc, argv, long_opts, SIZEOF(long_opts));

    utils_init_log_file(long_opts[0].arg, LOG_DIR);

    fact_tree_t ftree = {
        .root = NULL,
        .size = 0,
    };

    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    fact_tree_ctor(&ftree);

    fact_tree_insert(&ftree);
    fact_tree_insert(&ftree);
    fact_tree_insert(&ftree);

    fact_tree_dtor(&ftree);

    utils_end_log();

    return EXIT_SUCCESS;
}
