#include "ui_signal_dissect_view.h"

#ifndef UI_MAIN_SCREEN_H_
#define UI_MAIN_SCREEN_H_

typedef struct {
    GtkWidget *window;
    GtkWidget *ipentry;
    GtkWidget *portentry;

    GtkWidget      *progressbar;
    GtkWidget      *signalslist;
    ui_text_view_t *text_view;

    /* Buttons */
    GtkToolItem *connect;
    GtkToolItem *disconnect;

    int pipe_fd[2];
} ui_main_data_t;

extern ui_main_data_t ui_main_data;

int ui_gtk_initialize(int argc, char *argv[]);

#endif /* UI_MAIN_SCREEN_H_ */