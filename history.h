#ifndef HISTORY_H
#define HISTORY_H

extern int history_requested;
extern char *historyCmdLine;

int history_init();
int history_add(BYTE *cmdline);
int history_show();
int history_absolute_line(int x);
int history_relative_line(int x);
#endif
