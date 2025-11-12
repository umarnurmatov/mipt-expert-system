#include <ncurses.h>
#include <panel.h>

#include "fact_tree.h"
#include "optutils.h"
#include "memutils.h"
#include "utils.h"
#include "logutils.h"
#include "ioutils.h"

#define LOG_CATEGORY_OPT "OPTIONS"
#define LOG_CATEGORY_APP "APP"

static utils_long_opt_t long_opts[] = 
{
    { OPT_ARG_REQUIRED, "log", NULL, 0, 0 },
    { OPT_ARG_REQUIRED, "db" , NULL, 0, 0 },
};

typedef enum app_state_t 
{
    APP_STATE_MENU,
    APP_STATE_LOAD,
    APP_STATE_SAVE,
    APP_STATE_GUESS,
    APP_STATE_DEFINITION,
    APP_STATE_EXIT
} app_state_t;

typedef struct app_data_t
{
    app_state_t state;
    fact_tree_t* ftree;
    int exit;
} app_data_t;

typedef void (*app_callback_t) (app_data_t*);

typedef struct app_t 
{
    app_state_t state;
    app_callback_t callback;
} app_t;

void app_callback_menu       (app_data_t* adata);
void app_callback_load       (app_data_t* adata);
void app_callback_save       (app_data_t* adata);
void app_callback_guess      (app_data_t* adata);
void app_callback_definition (app_data_t* adata);
void app_callback_exit       (app_data_t* adata);

static app_t app[] =
{
    { APP_STATE_MENU,       app_callback_menu       },
    { APP_STATE_LOAD,       app_callback_load       },
    { APP_STATE_SAVE,       app_callback_save       },
    { APP_STATE_GUESS,      app_callback_guess      },
    { APP_STATE_DEFINITION, app_callback_definition },
    { APP_STATE_EXIT,       app_callback_exit       }
};

int main(int argc, char* argv[])
{
    utils_long_opt_get(argc, argv, long_opts, SIZEOF(long_opts));

    if(!long_opts[0].is_set) {
        return EXIT_FAILURE;
    }

    if(!long_opts[1].is_set) {
        return EXIT_FAILURE;
    }

    utils_init_log_file(long_opts[0].arg, LOG_DIR);

    fact_tree_t ftree = FACT_TREE_INIT_LIST;

    fact_tree_err_t err = FACT_TREE_ERR_NONE;
    err = fact_tree_ctor(&ftree);
    
    app_data_t appdata = {
        .state = APP_STATE_MENU,
        .ftree = &ftree,
        .exit = 0
    };

    while(!appdata.exit) {
        for(size_t i = 0; i < SIZEOF(app); ++i) {
            if(appdata.state == app[i].state) {
                system("clear");
                app[i].callback(&appdata);
                break;
            }
        }
    }

    fact_tree_dtor(&ftree);

    utils_end_log();

    return EXIT_SUCCESS;
}

void app_callback_menu(app_data_t* adata)
{
    printf("Modes\n"
           "1. Guess\n"
           "2. Load from file\n"
           "3. Save to file\n"
           "4. Get defition\n"
           "5. Exit\n"
           "Enter mode number: "
    );

    int input = 0;
    scanf("%d", &input);
    clear_stdin_buffer();
    
    switch(input) {
        case 1:
            adata->state = APP_STATE_GUESS;
            break;
        case 2:
            adata->state = APP_STATE_LOAD;
            break;
        case 3:
            adata->state = APP_STATE_SAVE;
            break;
        case 4:
            adata->state = APP_STATE_DEFINITION;
            break;
        case 5:
            adata->state = APP_STATE_EXIT;
            break;
        default:
            break;
    }
}

void app_callback_load(app_data_t* adata)
{
    printf("Enter file name: ");

    utils_str_t str = { NULL, 0 };
    input_string_until_correct(&str.str, &str.len);

    fact_tree_err_t err = fact_tree_fread(adata->ftree, str.str);
    if(err != FACT_TREE_ERR_NONE) {
        UTILS_LOGE(LOG_CATEGORY_APP, "%s", fact_tree_strerr(err));
    }

    printf("Press any key to continue...");
    scanf("%*c");
    
    adata->state = APP_STATE_MENU;

    NFREE(str.str);
}

void app_callback_save(app_data_t* adata)
{
    printf("Enter file name: ");
    utils_str_t str = { NULL, 0 };
    input_string_until_correct(&str.str, &str.len);

    fact_tree_err_t err = fact_tree_fwrite(adata->ftree, str.str);
    if(err != FACT_TREE_ERR_NONE) {
        UTILS_LOGE(LOG_CATEGORY_APP, "%s", fact_tree_strerr(err));
    }

    printf("Press any key to continue...");
    scanf("%*c");
    
    adata->state = APP_STATE_MENU;

    NFREE(str.str);
}

#define CHAR_ACCEPT 'y'
#define CHAR_DECLINE 'n'

void app_callback_guess(app_data_t* adata)
{
    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    fact_tree_node_t* node = fact_tree_guess(adata->ftree);

    printf("Is it %s? [y/N]: ", node->name.str);

    char input = CHAR_DECLINE;
    scanf("%c", &input);
    clear_stdin_buffer();

    fact_tree_node_t* new_node = NULL;
    if(input == CHAR_DECLINE) {
        err = fact_tree_insert(adata->ftree, node, &new_node);
        if(err != FACT_TREE_ERR_NONE) {
            UTILS_LOGE(LOG_CATEGORY_APP, "%s", fact_tree_strerr(err));
        }
    }

    printf("Press any key to continue...");
    scanf("%*c");

    adata->state = APP_STATE_MENU;

}

#undef CHAR_ACCEPT
#undef CHAR_DECLINE

void app_callback_definition(app_data_t* adata)
{
    printf("Enter object name: ");
    utils_str_t name = { NULL, 0 };
    input_string_until_correct(&name.str, &name.len);

    const fact_tree_node_t* node 
        = fact_tree_find_entity(adata->ftree->root, name.str);
    
    BEGIN {

        if(!node) {
            puts("No such object found!");
            GOTO_END;
        }

        char* definition = fact_tree_get_definition(node);

        if(!definition) GOTO_END;

        printf("%s\n", definition);
        NFREE(definition);

    } END;

    NFREE(name.str);

    printf("Press any key to continue...");
    scanf("%*c");

    adata->state = APP_STATE_MENU;
}

void app_callback_exit(app_data_t* adata)
{
    adata->exit = 1;
}
