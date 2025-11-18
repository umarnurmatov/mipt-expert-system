#include <SDL3/SDL_init.h>
#include <festival/festival.h>
#include <speech_tools/EST_String.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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
    { OPT_ARG_OPTIONAL, "db" , NULL, 0, 0 },
};

typedef enum app_state_t 
{
    APP_STATE_MENU,
    APP_STATE_LOAD,
    APP_STATE_SAVE,
    APP_STATE_GUESS,
    APP_STATE_DEFINITION,
    APP_STATE_DIFFERENCE,
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
void app_callback_difference (app_data_t* adata);
void app_callback_exit       (app_data_t* adata);

static app_t app_state[] =
{
    { APP_STATE_MENU,       app_callback_menu       },
    { APP_STATE_LOAD,       app_callback_load       },
    { APP_STATE_SAVE,       app_callback_save       },
    { APP_STATE_GUESS,      app_callback_guess      },
    { APP_STATE_DEFINITION, app_callback_definition },
    { APP_STATE_DIFFERENCE, app_callback_difference },
    { APP_STATE_EXIT,       app_callback_exit       }
};

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static ImGuiIO* io = NULL;
static fact_tree_t ftree = FACT_TREE_INIT_LIST;
static app_data_t appdata = {
    .state = APP_STATE_MENU,
    .ftree = &ftree
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create window with SDL_Renderer graphics context
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window = SDL_CreateWindow("Dear ImGui SDL3+SDL_Renderer example", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);

    if (window == nullptr)
    {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO(); (void)io;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    utils_long_opt_get(argc, argv, long_opts, SIZEOF(long_opts));

    if(!long_opts[0].is_set) {
        return SDL_APP_FAILURE;
    }

    utils_init_log_file(long_opts[0].arg, LOG_DIR);

    // Festival documentation recommend use such default values
    const int festival_load = 1;
    const int festival_buffer = 2100000;
    festival_initialize(festival_load, festival_buffer);


    fact_tree_err_t err = FACT_TREE_ERR_NONE;
    err = fact_tree_ctor(&ftree);
    if(err != FACT_TREE_ERR_NONE) {
        UTILS_LOGE(LOG_CATEGORY_APP, "%s", fact_tree_strerr(err));
    }

    if(long_opts[1].is_set)
        err = fact_tree_fread(&ftree, long_opts[1].arg);
    else
        err = fact_tree_fread(&ftree, "db.txt");

    if(err != FACT_TREE_ERR_NONE) {
        UTILS_LOGE(LOG_CATEGORY_APP, "%s", fact_tree_strerr(err));
    }


    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) 
{
    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();


    for(size_t i = 0; i < SIZEOF(app_state); ++i) {
        if(appdata.state == app_state[i].state) {
            app_state[i].callback(&appdata);
            break;
        }
    }

    // Rendering
    ImGui::Render();
    SDL_SetRenderScale(renderer, io->DisplayFramebufferScale.x, io->DisplayFramebufferScale.y);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    fact_tree_dtor(&ftree);
    utils_end_log();

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void app_callback_menu(app_data_t* adata)
{
    ImGui::Begin("Main menu");

    if(ImGui::Button("Guess"))
        adata->state = APP_STATE_GUESS;
    else if(ImGui::Button("Load database"))
        adata->state = APP_STATE_LOAD;
    else if(ImGui::Button("Save database"))
        adata->state = APP_STATE_SAVE;
    else if(ImGui::Button("Get definition"))
        adata->state = APP_STATE_DEFINITION;
    else if(ImGui::Button("Get difference"))
        adata->state = APP_STATE_DIFFERENCE;
    
    ImGui::End();
}

void app_callback_load(app_data_t* adata)
{
    printf_and_say("Enter file name: ");

    utils_str_t str = { NULL, 0 };
    input_string_until_correct(&str.str, &str.len);

    fact_tree_err_t err = fact_tree_fread(adata->ftree, str.str);
    if(err != FACT_TREE_ERR_NONE)
        UTILS_LOGE(LOG_CATEGORY_APP, "%s", fact_tree_strerr(err));

    printf_and_say("Press any key to continue...");
    scanf("%*c");
    
    adata->state = APP_STATE_MENU;

    NFREE(str.str);
}

void app_callback_save(app_data_t* adata)
{
    printf_and_say("Enter file name: ");
    utils_str_t str = { NULL, 0 };
    input_string_until_correct(&str.str, &str.len);

    fact_tree_err_t err = fact_tree_fwrite(adata->ftree, str.str);
    if(err != FACT_TREE_ERR_NONE) {
        UTILS_LOGE(LOG_CATEGORY_APP, "%s", fact_tree_strerr(err));
    }

    printf_and_say("Press any key to continue...");
    scanf("%*c");
    
    adata->state = APP_STATE_MENU;

    NFREE(str.str);
}

#define CHAR_ACCEPT 'y'
#define STR_ACCEPT "y"

#define CHAR_DECLINE 'n'
#define STR_DECLINE "n"

void app_callback_guess(app_data_t* adata)
{
    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    ImGui::Begin("Guess");

    fact_tree_node_t* node = fact_tree_guess(adata->ftree);

    BEGIN {

        if(!node) GOTO_END;

        ImGui::Text("Is it %s?", node->name.str);
        
        if(ImGui::Button("Yes")) GOTO_END;

        fact_tree_node_t* new_node = NULL;
        if(ImGui::Button("No")) {
            err = fact_tree_insert(adata->ftree, node, &new_node);
            if(err != FACT_TREE_ERR_NONE) {
                UTILS_LOGE(LOG_CATEGORY_APP, "%s", fact_tree_strerr(err));
            }
        }

        FACT_TREE_DUMP(adata->ftree, err);

    } END;

    adata->state = APP_STATE_MENU;

    ImGui::End();

}

#undef CHAR_ACCEPT
#undef CHAR_DECLINE

void app_callback_definition(app_data_t* adata)
{
    printf_and_say("Enter object name: ");

    utils_str_t name = { NULL, 0 };
    input_string_until_correct(&name.str, &name.len);

    const fact_tree_node_t* node 
        = fact_tree_find_object(adata->ftree->root, name.str);
    
    BEGIN {

        if(!node) {
            printf_and_say("No such object found\n");
            GOTO_END;
        }
        fact_tree_print_definition(adata->ftree, node);

    } END;

    NFREE(name.str);

    printf_and_say("Press any key to continue...");
    scanf("%*c");

    adata->state = APP_STATE_MENU;
}

void app_callback_difference(app_data_t* adata)
{
    utils_str_t name_a = { NULL, 0 };
    utils_str_t name_b = { NULL, 0 };

    BEGIN {

        if(adata->ftree->size < 1) {
            printf_and_say("Less than 1 object in tree!\n");
            GOTO_END;
        }

        printf_and_say("Enter first object name: ");
        input_string_until_correct(&name_a.str, &name_a.len);

        const fact_tree_node_t* node_a
            = fact_tree_find_object(adata->ftree->root, name_a.str);

        if(!node_a) {
            printf_and_say("No such object found!\n");
            GOTO_END;
        }

        printf_and_say("Enter second object name: ");
        input_string_until_correct(&name_b.str, &name_b.len);

        const fact_tree_node_t* node_b
            = fact_tree_find_object(adata->ftree->root, name_b.str);

        if(!node_b) {
            printf_and_say("No such object found!\n");
            GOTO_END;
        }

        if(node_a == node_b) {
            printf_and_say("Entered same objects!\n");
            GOTO_END;
        }

        fact_tree_print_difference(adata->ftree, node_a, node_b);

    } END;

    NFREE(name_a.str);
    NFREE(name_b.str);

    printf_and_say("Press any key to continue...");
    scanf("%*c");

    adata->state = APP_STATE_MENU;
}

void app_callback_exit(app_data_t* adata)
{
    adata->exit = 1;
}
