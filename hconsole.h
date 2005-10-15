//////////////////////////////////////////////////////////////////////////////////////////
//   hconsole.h        Hercules hardware console (panel) support functions
//////////////////////////////////////////////////////////////////////////////////////////
// (c) Copyright "Fish" (David B. Trout), 2005. Released under the Q Public License
// (http://www.conmicro.cx/hercules/herclic.html) as modifications to Hercules.
//////////////////////////////////////////////////////////////////////////////////////////

#ifndef _HCONSOLE_H
#define _HCONSOLE_H

#define KBD_HOME                "\x1B[1~"
#define KBD_INSERT              "\x1B[2~"
#define KBD_DELETE              "\x1B[3~"
#define KBD_END                 "\x1B[4~"
#define KBD_PAGE_UP             "\x1B[5~"
#define KBD_PAGE_DOWN           "\x1B[6~"
#define KBD_UP_ARROW            "\x1B[A"
#define KBD_DOWN_ARROW          "\x1B[B"
#define KBD_RIGHT_ARROW         "\x1B[C"
#define KBD_LEFT_ARROW          "\x1B[D"

// Hercules console color codes...

#define  COLOR_BLACK           0
#define  COLOR_RED             1
#define  COLOR_GREEN           2
#define  COLOR_BLUE            3
#define  COLOR_CYAN            4
#define  COLOR_MAGENTA         5
#define  COLOR_YELLOW          6
#define  COLOR_DARK_GREY       7
#define  COLOR_LIGHT_GREY      8
#define  COLOR_LIGHT_RED       9
#define  COLOR_LIGHT_GREEN     10
#define  COLOR_LIGHT_BLUE      11
#define  COLOR_LIGHT_CYAN      12
#define  COLOR_LIGHT_MAGENTA   13
#define  COLOR_LIGHT_YELLOW    14
#define  COLOR_WHITE           15
#define  COLOR_DEFAULT_FG      16
#define  COLOR_DEFAULT_BG      17
#define  COLOR_DEFAULT_LIGHT   18

extern  int   set_screen_color ( FILE* confp, short herc_fore,  short herc_back  );

// screen positions are 1-based; row 1 == top line; col 1 == leftmost column

extern  int   set_screen_pos   ( FILE* confp, short rowY1, short colX1 );

extern  int   clear_screen     ( FILE* confp );
extern  int   erase_to_eol     ( FILE* confp );

// 'save_and_set' = 1 --> just what it says; 0 --> restore from saved value.
extern  int   set_or_reset_console_mode ( int keybrd_fd, short save_and_set );

extern  void  translate_keystroke( char kbbuf[], int* pkblen );

extern  int   console_beep( FILE* confp );
extern  int   get_console_dim( FILE* confp, int* rows, int* cols );
extern  int   set_console_cursor_shape( FILE* confp, int ins );

#endif // _HCONSOLE_H
