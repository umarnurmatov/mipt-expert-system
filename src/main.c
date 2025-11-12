#include <ncurses.h>
#include <panel.h>

#include "fact_tree.h"
#include "optutils.h"
#include "utils.h"
#include "logutils.h"

#define LOG_CATEGORY_OPT "OPTIONS"

#define WIDTH 30
#define HEIGHT 10 

static utils_long_opt_t long_opts[] = 
{
    { OPT_ARG_REQUIRED, "log", NULL, 0, 0 },
    { OPT_ARG_REQUIRED, "db" , NULL, 0, 0 },
};

const char *choices[] = 
{ 
    "Choice 1",
    "Choice 2",
    "Choice 3",
    "Choice 4",
    "Exit",
};

void print_menu(WINDOW *menu_win, int highlight);

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

    fact_tree_t ftree = {
        .root = NULL,
        .size = 0,
        .buf = {
            .ptr = NULL,
            .len = 0,
            .pos = 0
        }
    };

    fact_tree_err_t err = FACT_TREE_ERR_NONE;

    fact_tree_ctor(&ftree);

    FACT_TREE_DUMP(&ftree, err);

    fact_tree_fread(&ftree, long_opts[1].arg);

    UTILS_LOGI("", "%p", fact_tree_find_entity(ftree.root, "Petrovich"));

    char* s = fact_tree_get_definition(fact_tree_find_entity(ftree.root, "Petrovich"));
    UTILS_LOGI("", "%s", s);
    free(s);

	//    WINDOW *menu_win;
	//
	//    initscr();
	//
	//    int startx = 0;
	//    int starty = 0;
	//
	//    clear();
	// noecho();
	// cbreak();
	// startx = (80 - WIDTH) / 2;
	// starty = (24 - HEIGHT) / 2;
	//
	// menu_win = newwin(HEIGHT, WIDTH, starty, startx);
	// mvprintw(0, 0, "Use arrow keys to go up and down, Press enter to select a choice");
	// keypad(menu_win, TRUE);
	// refresh();
	//
	//    int highlight = 1;
	// int choice = 0;
	// print_menu(menu_win, highlight);
	//    for( ;; ) {	
	//        int c = wgetch(menu_win);
	//        switch(c)
	//        {	case KEY_UP:
	//                if(highlight == 1)
	//                    highlight = SIZEOF(choices);
	//                else
	//                    --highlight;
	//                break;
	//            case KEY_DOWN:
	//                if(highlight == SIZEOF(choices))
	//                    highlight = 1;
	//                else 
	//                    ++highlight;
	//                break;
	//            case '\n':
	//                choice = highlight;
	//                break;
	//            default:
	//                break;
	//        }
	//        print_menu(menu_win, highlight);
	//        if(choice != 0)
	//            break;
	//    }	
	//
	//    refresh();
	//    getch();
	// endwin();
	//
    fact_tree_dtor(&ftree);

    utils_end_log();

    return EXIT_SUCCESS;
}


void print_menu(WINDOW *menu_win, int highlight)
{
	int x = 2;
	int y = 2;
	box(menu_win, 0, 0);
	for(int i = 0; i < SIZEOF(choices); ++i) {	
        if(highlight == i + 1) {	
            wattron(menu_win, A_REVERSE); 
			mvwprintw(menu_win, y, x, "%s", choices[i]);
			wattroff(menu_win, A_REVERSE);
		}
		else
			mvwprintw(menu_win, y, x, "%s", choices[i]);
		++y;
	}
	wrefresh(menu_win);
}
