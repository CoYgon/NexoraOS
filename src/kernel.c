#include <stdint.h>
#include <stddef.h>

#include "limine.h"
#include "serial.h"
#include "framebuffer.h"
#include "memory.h"
#include "keyboard.h"
#include "mouse.h"
#include "vfs.h"
#include "pkg.h"
#include "blockdev.h"
#include "fat32.h"
#include "emrefs.h"

LIMINE_REQUESTS_START_MARKER;
LIMINE_BASE_REVISION(3);

static volatile struct limine_framebuffer_request fb_request LIMINE_REQUEST = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = 0
};

static volatile struct limine_memmap_request memmap_request LIMINE_REQUEST = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = 0
};

LIMINE_REQUESTS_END_MARKER;

/* EmreOS palette */
#define C_BG       0x008080
#define C_PANEL    0xC0C0C0
#define C_CARD     0xD4D0C8
#define C_BORDER   0x808080
#define C_GREEN    0x000080
#define C_WHITE    0xFFFFFF
#define C_MUTED    0x404040
#define C_BLACK    0x000000
#define C_RED      0x800000

#define SCREEN_W 1280
#define SCREEN_H 800

#define TOPBAR_H 56
#define TASKBAR_H 48

#define APP_COUNT 4

struct window_state {
    int open;
    int x;
    int y;
    int w;
    int h;
    int app_id;
};

static struct window_state window = {
    .open = 0,
    .x = 310,
    .y = 170,
    .w = 620,
    .h = 380,
    .app_id = 0
};

static int dragging;
static int start_menu_open;
static int drag_offset_x;
static int drag_offset_y;

static int mouse_x = 100;
static int mouse_y = 100;

static uint8_t previous_buttons;

static uint64_t screen_width;
static uint64_t screen_height;

/* ---------------------------------------------------------
   Geometry helpers
   --------------------------------------------------------- */

static int point_in_rect(int x, int y,
                         int rx, int ry, int rw, int rh)
{
    return x >= rx && y >= ry &&
           x < rx + rw && y < ry + rh;
}

static int clamp_int(int value, int minimum, int maximum)
{
    if (maximum < minimum)
        maximum = minimum;

    if (value < minimum)
        return minimum;

    if (value > maximum)
        return maximum;

    return value;
}

/* ---------------------------------------------------------
   Mouse cursor
   --------------------------------------------------------- */

/* Save/restore the pixels under the software cursor. Redrawing the whole
 * desktop for every mouse packet caused visible CRT-like flicker. */
#define CURSOR_SAVE_W 18
#define CURSOR_SAVE_H 17
static uint32_t cursor_background[CURSOR_SAVE_H][CURSOR_SAVE_W];
static int cursor_saved_x, cursor_saved_y, cursor_saved_valid;

static void cursor_restore(void)
{
    if (!cursor_saved_valid) return;
    for (int y = 0; y < CURSOR_SAVE_H; ++y) {
        for (int x = 0; x < CURSOR_SAVE_W; ++x) {
            int px = cursor_saved_x + x;
            int py = cursor_saved_y + y;
            if (px >= 0 && py >= 0 && (uint64_t)px < screen_width &&
                (uint64_t)py < screen_height)
                fb_pixel((uint64_t)px, (uint64_t)py,
                         cursor_background[y][x]);
        }
    }
    cursor_saved_valid = 0;
}

static void cursor_save_background(int x, int y)
{
    cursor_saved_x = x - 1;
    cursor_saved_y = y - 1;
    for (int sy = 0; sy < CURSOR_SAVE_H; ++sy) {
        for (int sx = 0; sx < CURSOR_SAVE_W; ++sx) {
            int px = cursor_saved_x + sx;
            int py = cursor_saved_y + sy;
            cursor_background[sy][sx] = 0;
            if (px >= 0 && py >= 0 && (uint64_t)px < screen_width &&
                (uint64_t)py < screen_height) {
                /* Read through the accessor so framebuffer pitch is respected. */
                cursor_background[sy][sx] = fb_read_pixel((uint64_t)px, (uint64_t)py);
            }
        }
    }
    cursor_saved_valid = 1;
}

static const uint16_t cursor_shape[16] = {
    0x8000,
    0xC000,
    0xE000,
    0xF000,
    0xF800,
    0xFC00,
    0xFE00,
    0xFF00,
    0xF800,
    0xDC00,
    0x8E00,
    0x0600,
    0x0700,
    0x0300,
    0x0300,
    0x0000
};

static void draw_cursor(int x, int y)
{
    cursor_save_background(x, y);
    for (int row = 0; row < 15; ++row) {
        for (int col = 0; col < 16; ++col) {
            if (!(cursor_shape[row] & (0x8000u >> col)))
                continue;

            int px = x + col;
            int py = y + row;

            /* Dark outline around the white cursor */
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (ox == 0 && oy == 0)
                        continue;

                    int nx = px + ox;
                    int ny = py + oy;

                    if (nx >= 0 && ny >= 0 &&
                        (uint64_t)nx < screen_width &&
                        (uint64_t)ny < screen_height) {
                        fb_pixel((uint64_t)nx,
                                 (uint64_t)ny,
                                 C_BLACK);
                    }
                }
            }

            if (px >= 0 && py >= 0 &&
                (uint64_t)px < screen_width &&
                (uint64_t)py < screen_height) {
                fb_pixel((uint64_t)px,
                         (uint64_t)py,
                         C_WHITE);
            }
        }
    }
}

/* ---------------------------------------------------------
   Desktop
   --------------------------------------------------------- */

static uint64_t content_x(void)
{
    return 36;
}

static uint64_t content_width(void)
{
    uint64_t x = content_x();

    if (screen_width <= x + 24)
        return 1;

    return screen_width - x - 24;
}

static uint64_t app_card_width(void)
{
    uint64_t cw = content_width();

    if (cw <= 32)
        return 1;

    return (cw - 32) / APP_COUNT;
}

static uint64_t app_card_x(int app)
{
    return content_x() + (uint64_t)app *
           (app_card_width() + 16);
}

static void draw_topbar(void)
{
    fb_rect(0, 0, screen_width, TOPBAR_H, C_PANEL);
    fb_rect(0, 0, screen_width, 2, C_WHITE);
    fb_rect(0, TOPBAR_H - 2, screen_width, 2, C_BORDER);
    fb_text(18, 20, "EMREOS CLASSIC DESKTOP", C_GREEN, 1);
    fb_text(screen_width > 200 ? screen_width - 150 : 8, 20, "X64 SYSTEM", C_MUTED, 1);
}

static void draw_app_cards(void)
{
    const char *names[APP_COUNT] = {
        "THIS PC", "FILES", "DISPLAY", "TERMINAL"
    };

    const char *descriptions[APP_COUNT] = {
        "SYSTEM INFORMATION", "FILE MANAGER", "DISPLAY SETTINGS", "COMMAND SHELL"
    };

    uint64_t y = 158;
    uint64_t h = 112;
    uint64_t cw = app_card_width();

    for (int i = 0; i < APP_COUNT; ++i) {
        uint64_t x = app_card_x(i);

        fb_rect(x, y, cw, h, C_PANEL);
        fb_rect(x, y, cw, 2, C_GREEN);
        /* Simple desktop app icon */
        fb_rect(x + 14, y + 18, 34, 30, i == 1 ? 0xD5A84B : C_GREEN);
        fb_rect(x + 18, y + 22, 26, 20, C_CARD);
        fb_text(x + 14, y + 57, names[i], C_WHITE, 1);
        fb_text(x + 14, y + 76, descriptions[i], C_MUTED, 1);
    }
}

static void draw_status_panel(void)
{
    uint64_t x = content_x();
    uint64_t y = 296;
    uint64_t w = content_width();

    if (y + 100 >= screen_height - TASKBAR_H)
        return;

    fb_rect(x, y, w,
            screen_height - TASKBAR_H - y - 18,
            C_PANEL);

    fb_text(x + 18, y + 18,
            "SYSTEM OVERVIEW", C_GREEN, 1);

    fb_rect(x + 18, y + 43, w - 36,
            1, C_BORDER);

    fb_text(x + 18, y + 60,
            "LIMINE BOOT OK", C_WHITE, 1);

    fb_text(x + 18, y + 84,
            "FRAMEBUFFER ACTIVE", C_WHITE, 1);

    fb_text(x + 18, y + 108,
            "PS/2 INPUT ENABLED", C_WHITE, 1);
}

static void draw_taskbar(void)
{
    fb_rect(0, screen_height - TASKBAR_H,
            screen_width, TASKBAR_H, C_PANEL);
    fb_rect(0, screen_height - TASKBAR_H,
            screen_width, 2, C_WHITE);

    /* Windows-inspired Start button and pinned app buttons */
    uint64_t by = screen_height - 38;
    fb_rect(8, by - 1, 38, 30, C_PANEL);
    fb_rect(8, by - 1, 38, 2, C_WHITE);
    fb_rect(8, by + 27, 38, 2, C_BORDER);
    fb_rect(12, by, 28, 28, C_CARD);
    fb_rect(18, by + 6, 8, 8, C_GREEN);
    fb_rect(28, by + 6, 6, 8, C_GREEN);
    fb_rect(18, by + 16, 8, 6, C_GREEN);
    fb_rect(28, by + 16, 6, 6, C_GREEN);
    fb_rect(50, by, 104, 28, C_CARD);
    fb_text(62, by + 10, "FILES", C_WHITE, 1);
    fb_rect(162, by, 126, 28, C_CARD);
    fb_text(174, by + 10, "TERMINAL", C_WHITE, 1);
    fb_text(screen_width - 165, by + 10,
            "EMREOS  X64", C_MUTED, 1);
}

/* ---------------------------------------------------------
   Window rendering
   --------------------------------------------------------- */

static void constrain_window(void)
{
    int max_x = (int)screen_width - window.w;
    int max_y = (int)screen_height - TASKBAR_H - window.h;

    window.x = clamp_int(window.x, 0, max_x);
    window.y = clamp_int(window.y, TOPBAR_H, max_y);
}


#define SHELL_LINE_MAX 96
#define SHELL_HISTORY 10
#define KEY_UP 0x101
#define KEY_DOWN 0x102
#define SHELL_DATA_MAX 192
static char shell_input[SHELL_LINE_MAX];
static size_t shell_input_len;
static char shell_lines[SHELL_HISTORY][SHELL_LINE_MAX];
static char shell_history[SHELL_HISTORY][SHELL_LINE_MAX];
static unsigned history_count, history_pos;
static unsigned shell_line_count;

static void shell_line(const char *text)
{
    if (shell_line_count < SHELL_HISTORY) {
        size_t i = 0;
        while (text && text[i] && i < SHELL_LINE_MAX - 1) {
            shell_lines[shell_line_count][i] = text[i]; ++i;
        }
        shell_lines[shell_line_count][i] = 0;
        ++shell_line_count;
    } else {
        for (unsigned r = 1; r < SHELL_HISTORY; ++r) {
            size_t i = 0;
            while (i < SHELL_LINE_MAX - 1 && (shell_lines[r-1][i] = shell_lines[r][i])) ++i;
            shell_lines[r-1][i] = 0;
        }
        size_t i = 0;
        while (text && text[i] && i < SHELL_LINE_MAX - 1) { shell_lines[SHELL_HISTORY-1][i] = text[i]; ++i; }
        shell_lines[SHELL_HISTORY-1][i] = 0;
    }
}

static size_t shell_text_len(const char *s) { size_t n=0; while (s && s[n]) ++n; return n; }

static int shell_equal(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == 0 && *b == 0;
}

static void shell_execute(void)
{
    shell_input[shell_input_len] = 0;
    shell_line("> ");
    if (shell_line_count) {
        unsigned row = shell_line_count - 1;
        size_t i = 0;
        while (shell_lines[row][i] && i < SHELL_LINE_MAX - 1) ++i;
        for (size_t j = 0; shell_input[j] && i < SHELL_LINE_MAX - 1; ++j)
            shell_lines[row][i++] = shell_input[j];
        shell_lines[row][i] = 0;
    }

    if (shell_input_len) {
        if (history_count < SHELL_HISTORY) {
            size_t k=0; while(shell_input[k] && k<SHELL_LINE_MAX-1){shell_history[history_count][k]=shell_input[k];k++;} shell_history[history_count++][k]=0;
        } else {
            for(unsigned h=1;h<SHELL_HISTORY;h++){size_t k=0;while(shell_history[h][k]&&k<SHELL_LINE_MAX-1){shell_history[h-1][k]=shell_history[h][k];k++;}shell_history[h-1][k]=0;}
            size_t k=0;while(shell_input[k]&&k<SHELL_LINE_MAX-1){shell_history[SHELL_HISTORY-1][k]=shell_input[k];k++;}shell_history[SHELL_HISTORY-1][k]=0;
        }
    }
    history_pos=history_count;
    if (!shell_input_len) {
        shell_line("");
    } else if (shell_equal(shell_input, "help")) {
        shell_line("help clear echo <text> uname mem pages df");
        shell_line("ls cd <dir> pwd mkdir <dir> touch <file>");
        shell_line("cat <file> write <file> <text> (RAM only)");
        shell_line("rm <file> rmdir <empty-dir> mv <old> <new>");
        shell_line("pkg help/search/list/install/remove/update/upgrade/store");
        shell_line("disk mounts fatls [path] fatcat <path> fsinfo");
        shell_line("emrelist emrecat <name> emremkdir <name> emrewrite <name> <text>");
        shell_line("history version about exit");
    } else if (shell_equal(shell_input, "clear")) {
        shell_line_count = 0;
    } else if (shell_equal(shell_input, "ls")) {
        char out[SHELL_DATA_MAX];
        vfs_ls(out, sizeof(out));
        char *line=out; for(char *q=out;;q++){if(*q=='\n'||!*q){char saved=*q;*q=0;if(*line)shell_line(line);if(!saved)break;line=q+1;}}
    } else if (shell_equal(shell_input, "pwd")) {
        shell_line(vfs_pwd());
    } else if (shell_equal(shell_input, "history")) {
        for(unsigned h=0;h<history_count;h++) shell_line(shell_history[h]);
    } else if (shell_input_len > 3 && shell_input[0]=='c' && shell_input[1]=='d' && shell_input[2]==' ') {
        shell_line(vfs_cd(shell_input+3)==0 ? "directory changed" : "cd: directory not found");
    } else if (shell_input_len > 4 && shell_input[0]=='c' && shell_input[1]=='a' && shell_input[2]=='t' && shell_input[3]==' ') {
        char out[SHELL_DATA_MAX]; if(vfs_cat(shell_input+4,out,sizeof(out))<0) shell_line("cat: file not found or directory"); else shell_line(out[0]?out:"(empty file)");
    } else if (shell_input_len > 6 && shell_input[0]=='m' && shell_input[1]=='k' && shell_input[2]=='d' && shell_input[3]=='i' && shell_input[4]=='r' && shell_input[5]==' ') {
        shell_line(vfs_mkdir(shell_input+6)==0 ? "directory created" : "mkdir: invalid name or no space");
    } else if (shell_input_len > 6 && shell_input[0]=='t' && shell_input[1]=='o' && shell_input[2]=='u' && shell_input[3]=='c' && shell_input[4]=='h' && shell_input[5]==' ') {
        shell_line(vfs_touch(shell_input+6)==0 ? "file created" : "touch: invalid name or no space");
    } else if (shell_input_len > 7 && shell_input[0]=='w' && shell_input[1]=='r' && shell_input[2]=='i' && shell_input[3]=='t' && shell_input[4]=='e' && shell_input[5]==' ') {
        char *arg=shell_input+6; char *space=arg; while(*space && *space!=' ')space++;
        if(!*space) shell_line("usage: write <file> <text>");
        else { *space=0; shell_line(vfs_write(arg,space+1)==0 ? "file written to RAM filesystem" : "write: failed"); }
    } else if (shell_input_len > 3 && shell_input[0]=='r' && shell_input[1]=='m' && shell_input[2]==' ') {
        shell_line(vfs_rm(shell_input+3)==0 ? "file removed" : "rm: file not found or is a directory");
    } else if (shell_input_len > 6 && shell_input[0]=='r' && shell_input[1]=='m' && shell_input[2]=='d' && shell_input[3]=='i' && shell_input[4]=='r' && shell_input[5]==' ') {
        shell_line(vfs_rmdir(shell_input+6)==0 ? "empty directory removed" : "rmdir: directory missing, not empty, or current");
    } else if (shell_input_len > 3 && shell_input[0]=='m' && shell_input[1]=='v' && shell_input[2]==' ') {
        char *args=shell_input+3; char *space=args; while(*space && *space!=' ') ++space;
        if (!*space) shell_line("usage: mv <old-name> <new-name>");
        else { *space++=0; while(*space==' ') ++space; shell_line(vfs_rename(args,space)==0 ? "renamed" : "mv: invalid names or destination exists"); }
    } else if (shell_equal(shell_input, "df")) {
        char out[32]; unsigned used=vfs_used_nodes(), total=vfs_total_nodes();
        size_t n=0; unsigned vals[2]={used,total};
        for (unsigned z=0; z<2; ++z) { unsigned v=vals[z]; char rev[12]; size_t r=0; do { rev[r++]=(char)('0'+v%10); v/=10; } while(v && r<sizeof(rev)); while(r) out[n++]=rev[--r]; if(!z){out[n++]='/';} }
        out[n]=0; shell_line("RAM VFS NODES USED / TOTAL:"); shell_line(out);
    } else if (shell_equal(shell_input, "pkg") ||
               (shell_input_len > 4 && shell_input[0]=='p' && shell_input[1]=='k' && shell_input[2]=='g' && shell_input[3]==' ')) {
        const char *args = shell_equal(shell_input, "pkg") ? "" : shell_input + 4;
        char out[SHELL_DATA_MAX];
        if (pkg_execute(args, out, sizeof(out))) {
            char *line = out;
            for (char *q = out;; ++q) {
                if (*q == '\n' || *q == 0) {
                    char saved = *q; *q = 0;
                    if (*line) shell_line(line);
                    if (!saved) break;
                    line = q + 1;
                }
            }
        }
    } else if (shell_equal(shell_input, "disk")) {
        const struct blockdev_info *d=blockdev_get_info();
        if(!d->present) shell_line("No supported ATA PIO disk detected (primary IDE channel only).");
        else { shell_line(d->name); shell_line(d->model); char num[32]; size_t n=0; uint64_t v=d->sectors; if(!v)num[n++]='0'; while(v&&n<sizeof(num)-1){num[n++]=(char)('0'+v%10);v/=10;} for(size_t i=0;i<n/2;++i){char t=num[i];num[i]=num[n-1-i];num[n-1-i]=t;} num[n]=0; shell_line("Sector count:"); shell_line(num); }
    } else if (shell_equal(shell_input, "mounts")) {
        shell_line("ramfs: mounted (volatile, read/write)");
        shell_line(fat32_is_mounted() ? "fat32: mounted read-only (path navigation)" : "fat32: not mounted");
        shell_line(!emrefs_is_mounted() ? "emrefs: not mounted" : (emrefs_is_writable() ? "emrefs: mounted read/write" : "emrefs: mounted read-only (device has no write driver)"));
    } else if (shell_equal(shell_input, "fatls") || (shell_input_len > 6 && shell_input[0]=='f'&&shell_input[1]=='a'&&shell_input[2]=='t'&&shell_input[3]=='l'&&shell_input[4]=='s'&&shell_input[5]==' ')) {
        const char *path = shell_equal(shell_input, "fatls") ? "/" : shell_input + 6;
        char out[SHELL_DATA_MAX]; int n=fat32_list_path(path,out,sizeof(out));
        if(n<0) shell_line("fatls: directory missing, unsupported path, or read error");
        else { shell_line("FAT32 directory (read-only, 8.3 names):"); char *line=out; for(char *q=out;;++q){if(*q=='\n'||!*q){char saved=*q;*q=0;if(*line)shell_line(line);if(!saved)break;line=q+1;}} }
    } else if (shell_input_len > 7 && shell_input[0]=='f'&&shell_input[1]=='a'&&shell_input[2]=='t'&&shell_input[3]=='c'&&shell_input[4]=='a'&&shell_input[5]=='t'&&shell_input[6]==' ') {
        char out[SHELL_DATA_MAX]; unsigned bytes=0; int rc=fat32_read_file(shell_input+7,out,sizeof(out),&bytes); if(rc<0) shell_line("fatcat: file not found, unsupported name, or disk read error"); else { shell_line(out); if(rc>0) shell_line("[output truncated to terminal buffer]"); }
    } else if (shell_equal(shell_input, "emrelist")) {
        char out[SHELL_DATA_MAX]; int n=emrefs_list_root(out,sizeof(out)); if(n<0)shell_line("emrelist: EmreFS not mounted or read error");else{char *line=out;for(char *q=out;;++q){if(*q=='\n'||!*q){char saved=*q;*q=0;if(*line)shell_line(line);if(!saved)break;line=q+1;}}}
    } else if (shell_input_len>8 && shell_input[0]=='e'&&shell_input[1]=='m'&&shell_input[2]=='r'&&shell_input[3]=='e'&&shell_input[4]=='c'&&shell_input[5]=='a'&&shell_input[6]=='t'&&shell_input[7]==' ') {
        char out[SHELL_DATA_MAX];uint32_t bytes=0;if(!emrefs_read_file(shell_input+8,out,sizeof(out)-1,&bytes))shell_line("emrecat: file not found, buffer too small, or volume unavailable");else{out[bytes]=0;shell_line(out);}
    } else if (shell_input_len>9 && shell_input[0]=='e'&&shell_input[1]=='m'&&shell_input[2]=='r'&&shell_input[3]=='e'&&shell_input[4]=='m'&&shell_input[5]=='k'&&shell_input[6]=='d'&&shell_input[7]=='i'&&shell_input[8]=='r'&&shell_input[9]==' ') {
        shell_line(emrefs_mkdir(shell_input+10)?"EmreFS directory created":"emremkdir: requires a mounted writable EmreFS volume");
    } else if (shell_input_len>10 && shell_input[0]=='e'&&shell_input[1]=='m'&&shell_input[2]=='r'&&shell_input[3]=='e'&&shell_input[4]=='w'&&shell_input[5]=='r'&&shell_input[6]=='i'&&shell_input[7]=='t'&&shell_input[8]=='e'&&shell_input[9]==' ') {
        char *arg=shell_input+10;char *space=arg;while(*space&&*space!=' ')++space;if(!*space)shell_line("usage: emrewrite <name> <text>");else{*space++=0;while(*space==' ')++space;shell_line(emrefs_write_file(arg,space,(uint32_t)shell_text_len(space))?"EmreFS file written":"emrewrite: requires a writable volume or available space");}
    } else if (shell_equal(shell_input, "fsinfo")) {
        shell_line("EmreFS V1: superblock, bitmap, root directory and contiguous file extents");
        shell_line(!emrefs_is_mounted()?"Volume: not mounted":(emrefs_is_writable()?"Volume: read/write":"Volume: read-only (block device is read-only)"));
    } else if (shell_equal(shell_input, "uname")) {
        shell_line("EMREOS X86_64 KERNEL");
    } else if (shell_equal(shell_input, "version")) {
        shell_line("EMREOS CLASSIC V6.1 (FAT32 PATHS / EMREFS V1)");
    } else if (shell_equal(shell_input, "about")) {
        shell_line("BARE-METAL OS FOR X86_64");
        shell_line("BUILT-IN SHELL; NOT A POSIX SHELL");
    } else if (shell_equal(shell_input, "mem")) {
        shell_line("USABLE MEMORY BYTES:");
        char num[24]; size_t n = 0;
        uint64_t v = memory_usable_bytes();
        if (!v) num[n++] = '0';
        while (v && n < sizeof(num)-1) { num[n++] = (char)('0' + v % 10); v /= 10; }
        for (size_t i=0; i<n/2; ++i) { char t=num[i]; num[i]=num[n-1-i]; num[n-1-i]=t; }
        num[n]=0; shell_line(num);
    } else if (shell_equal(shell_input, "pages")) {
        shell_line("PAGE SIZE: 4096 BYTES");
        shell_line("PHYSICAL PAGE ALLOCATOR ACTIVE");
        shell_line("PAGING / USER MODE: NOT YET ENABLED");
        shell_line("RAM VFS ACTIVE (VOLATILE)");
        shell_line("FAT32: READ-ONLY, LEGACY ATA PIO ONLY");
        shell_line("USER MODE / SCHEDULER: NOT YET ENABLED");
    } else if (shell_equal(shell_input, "exit")) {
        window.open = 0;
    } else if (shell_input_len >= 5 && shell_input[0]=='e' && shell_input[1]=='c' &&
               shell_input[2]=='h' && shell_input[3]=='o' && shell_input[4]==' ') {
        shell_line(shell_input + 5);
    } else {
        shell_line("UNKNOWN COMMAND. TYPE HELP.");
    }
    shell_input_len = 0;
    shell_input[0] = 0;
}

static void shell_key(int c)
{
    if (!window.open || window.app_id != 3) return;
    if (c == KEY_UP || c == KEY_DOWN) {
        if (!history_count) return;
        if(c==KEY_UP){if(history_pos>0)history_pos--;}else if(history_pos<history_count)history_pos++;
        if(history_pos<history_count){size_t k=0;while(shell_history[history_pos][k]&&k<SHELL_LINE_MAX-1){shell_input[k]=shell_history[history_pos][k];k++;}shell_input[k]=0;shell_input_len=k;}else{shell_input_len=0;shell_input[0]=0;}
        return;
    }
    if (c == '\n') shell_execute();
    else if (c == '\b') {
        if (shell_input_len) shell_input[--shell_input_len] = 0;
    } else if (c >= 32 && c <= 126 && shell_input_len < SHELL_LINE_MAX-1) {
        shell_input[shell_input_len++] = (char)c;
        shell_input[shell_input_len] = 0;
    }
}

static void draw_shell(void)
{
    int x = window.x + 20, y = window.y + 62;
    fb_rect((uint64_t)(x-8), (uint64_t)(y-8), (uint64_t)(window.w-24),
            (uint64_t)(window.h-78), C_BLACK);
    fb_text(x, y, "EMREOS BUILT-IN SHELL", C_GREEN, 1);
    y += 20;
    unsigned first = shell_line_count > 7 ? shell_line_count - 7 : 0;
    for (unsigned i=first; i<shell_line_count; ++i) {
        fb_text(x, y, shell_lines[i], C_WHITE, 1);
        y += 18;
    }
    if (y + 20 < window.y + window.h - 8) {
        fb_text(x, y, "> ", C_GREEN, 1);
        fb_text(x+12, y, shell_input, C_WHITE, 1);
        fb_rect((uint64_t)(x+12+(int)shell_input_len*6), (uint64_t)(y+8), 5, 1, C_GREEN);
    }
}

static void draw_window(void)
{
    if (!window.open)
        return;

    constrain_window();

    int x = window.x;
    int y = window.y;
    int w = window.w;
    int h = window.h;

    fb_rect(x + 5, y + 6, w, h, C_BLACK);
    fb_rect(x, y, w, h, C_PANEL);
    fb_rect(x, y, w, 2, C_WHITE);
    fb_rect(x, y, 2, h, C_WHITE);
    fb_rect(x + w - 2, y, 2, h, C_BORDER);
    fb_rect(x, y + h - 2, w, 2, C_BORDER);

    fb_rect(x, y, w, 36, C_GREEN);
    fb_rect(x, y, w, 2, C_WHITE);

    if (window.app_id == 0)
        fb_text(x + 15, y + 14,
                "MEMORY MANAGER", C_WHITE, 1);
    else if (window.app_id == 1)
        fb_text(x + 15, y + 14,
                "DISPLAY SETTINGS", C_WHITE, 1);
    else if (window.app_id == 2)
        fb_text(x + 15, y + 14, "FILE EXPLORER", C_WHITE, 1);
    else
        fb_text(x + 15, y + 14, "EMREOS TERMINAL", C_WHITE, 1);

    /* Close button */
    fb_rect(x + w - 34, y + 6, 26, 24, C_RED);
    fb_text(x + w - 26, y + 14, "X", C_WHITE, 1);

    fb_rect(x + 16, y + 52, w - 32,
            1, C_BORDER);

    if (window.app_id == 0) {
        fb_text(x + 20, y + 75,
                "PHYSICAL MEMORY", C_GREEN, 1);

        fb_text(x + 20, y + 110,
                "Page size: 4096 bytes", C_WHITE, 1);

        fb_text(x + 20, y + 140,
                "Memory map: received", C_WHITE, 1);

        fb_text(x + 20, y + 170,
                "Physical allocation: enabled", C_WHITE, 1);

        fb_text(x + 20, y + 200,
                "Virtual memory: not implemented", C_MUTED, 1);
    } else if (window.app_id == 1) {
        fb_text(x + 20, y + 75,
                "DISPLAY CONFIGURATION", C_GREEN, 1);

        fb_text(x + 20, y + 110,
                "Current framebuffer mode:", C_MUTED, 1);

        fb_text(x + 20, y + 140,
                "1280 X 800", C_WHITE, 1);

        fb_text(x + 20, y + 180,
                "Mode selection requires bootloader support",
                C_MUTED, 1);
    } else if (window.app_id == 2) {
        char listing[512];
        fb_text(x + 20, y + 75, "LOCATION", C_MUTED, 1);
        fb_rect(x + 92, y + 68, w - 116, 24, C_CARD);
        fb_text(x + 102, y + 77, vfs_pwd(), C_WHITE, 1);
        fb_text(x + 20, y + 112, "NAME", C_MUTED, 1);
        fb_text(x + 250, y + 112, "TYPE", C_MUTED, 1);
        fb_rect(x + 18, y + 130, w - 36, 1, C_BORDER);
        if (vfs_ls(listing, sizeof(listing)) >= 0) {
            int row = 0;
            char *line = listing;
            for (char *q = listing; ; ++q) {
                if (*q == '\n' || *q == 0) {
                    char saved = *q; *q = 0;
                    if (*line && row < 9) {
                        uint64_t ry = (uint64_t)(y + 143 + row * 22);
                        if ((row & 1) == 0) fb_rect(x + 18, ry - 3, w - 36, 20, 0x252D3A);
                        fb_rect(x + 26, ry, 12, 12, line[0] && line[0] != '(' ? 0xD5A84B : C_BORDER);
                        fb_text(x + 48, ry + 2, line, C_WHITE, 1);
                        fb_text(x + 250, ry + 2, (line[0] && line[0] != '(' && line[0] != '.' && line[0] != '/') ? "FILE / DIR" : "--", C_MUTED, 1);
                        row++;
                    }
                    if (!saved) break;
                    line = q + 1;
                }
            }
        }
        fb_text(x + 20, y + h - 24, "RAM FILE SYSTEM - CHANGES ARE VOLATILE", C_MUTED, 1);
    } else {
        draw_shell();
    }
}

static void draw_start_menu(void)
{
    if (!start_menu_open) return;
    int h = 226;
    int y = (int)screen_height - TASKBAR_H - h;
    if (y < TOPBAR_H) y = TOPBAR_H;
    fb_rect(4, y + 4, 246, h, C_BLACK);
    fb_rect(2, y + 2, 246, h, C_PANEL);
    fb_rect(2, y + 2, 246, 3, C_WHITE);
    fb_rect(2, y + 2, 3, h, C_WHITE);
    fb_rect(8, y + 10, 34, h - 20, C_GREEN);
    fb_text(17, y + 20, "E", C_WHITE, 2);
    fb_text(54, y + 18, "EMREOS PROGRAMS", C_GREEN, 1);
    fb_rect(50, y + 34, 184, 1, C_BORDER);
    fb_text(54, y + 50, "THIS PC - MEMORY", C_BLACK, 1);
    fb_text(54, y + 82, "FILE EXPLORER", C_BLACK, 1);
    fb_text(54, y + 114, "DISPLAY SETTINGS", C_BLACK, 1);
    fb_text(54, y + 146, "TERMINAL", C_BLACK, 1);
    fb_rect(50, y + 174, 184, 1, C_BORDER);
    fb_text(54, y + 190, "CLASSIC X64 DESKTOP", C_MUTED, 1);
}

static void draw_desktop(void)
{
    if (!fb_available())
        return;

    screen_width = fb_width();
    screen_height = fb_height();

    fb_clear(C_BG);

    draw_topbar();
    draw_app_cards();
    draw_status_panel();
    draw_taskbar();
    draw_start_menu();
    draw_window();

    /* Cursor is always rendered last. */
    draw_cursor(mouse_x, mouse_y);
}

/* ---------------------------------------------------------
   Mouse events
   --------------------------------------------------------- */

static void open_app(int app_id)
{
    window.open = 1;
    window.app_id = app_id;

    window.w = 620;
    window.h = 380;

    if (screen_width < 680)
        window.w = (int)screen_width - 20;

    if (screen_height < 440)
        window.h = (int)screen_height - TOPBAR_H - TASKBAR_H - 10;

    if (window.w < 250)
        window.w = 250;

    if (window.h < 160)
        window.h = 160;

    window.x = ((int)screen_width - window.w) / 2;
    window.y = ((int)screen_height - window.h) / 2;

    constrain_window();
}

static void handle_mouse_event(struct mouse_state mouse)
{
    int left_now = (mouse.buttons & 1) != 0;
    int left_before = (previous_buttons & 1) != 0;
    int pressed = left_now && !left_before;

    mouse_x = clamp_int(mouse.x, 0, (int)screen_width - 1);
    mouse_y = clamp_int(mouse.y, 0, (int)screen_height - 1);

    if (pressed) {
        if (window.open &&
            point_in_rect(mouse_x, mouse_y,
                          window.x + window.w - 36,
                          window.y, 36, 36)) {
            window.open = 0;
            dragging = 0;
        } else if (window.open &&
                   point_in_rect(mouse_x, mouse_y,
                                 window.x, window.y,
                                 window.w, 36)) {
            dragging = 1;
            drag_offset_x = mouse_x - window.x;
            drag_offset_y = mouse_y - window.y;
        } else {
            dragging = 0;
            int task_y = (int)screen_height - TASKBAR_H;
            int menu_y = (int)screen_height - TASKBAR_H - 226;
            if (mouse_y >= task_y && mouse_x >= 8 && mouse_x < 46) {
                start_menu_open = !start_menu_open;
            } else if (start_menu_open && mouse_x >= 2 && mouse_x < 248 && mouse_y >= menu_y && mouse_y < menu_y + 226) {
                if (mouse_y < menu_y + 78) open_app(0);
                else if (mouse_y < menu_y + 110) open_app(2);
                else if (mouse_y < menu_y + 142) open_app(1);
                else if (mouse_y < menu_y + 174) open_app(3);
                start_menu_open = 0;
            } else if (mouse_y >= task_y && mouse_x >= 50 && mouse_x < 154) {
                open_app(2); start_menu_open = 0;
            } else if (mouse_y >= task_y && mouse_x >= 162 && mouse_x < 290) {
                open_app(3); start_menu_open = 0;
            } else {
                start_menu_open = 0;
            uint64_t card_y = 158;
            uint64_t card_h = 112;
            uint64_t cw = app_card_width();

            for (int i = 0; i < APP_COUNT; ++i) {
                int x = (int)app_card_x(i);

                if (point_in_rect(mouse_x, mouse_y,
                                  x, (int)card_y,
                                  (int)cw, (int)card_h)) {
                    open_app(i == 1 ? 2 : (i == 2 ? 1 : i));
                    break;
                }
            }
            }
        }
    }

    if (!left_now)
        dragging = 0;

    if (dragging && left_now) {
        window.x = mouse_x - drag_offset_x;
        window.y = mouse_y - drag_offset_y;
        constrain_window();
    }

    previous_buttons = mouse.buttons;
}

/* ---------------------------------------------------------
   Kernel entry
   --------------------------------------------------------- */

void kmain(void)
{
    serial_init();

    serial_write("\n====================================\n");
    serial_write(" EmreOS V6 kernel booting (x86_64)\n");
    serial_write("====================================\n");

    if (limine_base_revision[2] == 0)
        serial_write("[boot] Limine base revision accepted\n");
    else
        serial_write("[boot] Limine base revision unsupported\n");

    if (fb_request.response &&
        fb_request.response->framebuffer_count > 0) {
        fb_init(fb_request.response->framebuffers[0]);

        screen_width = fb_width();
        screen_height = fb_height();

        serial_write("[framebuffer] ");
        serial_write_dec(screen_width);
        serial_write("x");
        serial_write_dec(screen_height);
        serial_write(" bpp=32\n");
    } else {
        fb_init(0);
        screen_width = 0;
        screen_height = 0;

        serial_write("[framebuffer] unavailable\n");
    }

    if (memmap_request.response) {
        memory_init(memmap_request.response);

        serial_write("[memory] usable bytes: ");
        serial_write_dec(memory_usable_bytes());
        serial_write("\n");

        serial_write("[memory] first page: ");
        serial_write_hex(page_alloc());
        serial_write("\n");
    } else {
        memory_init(0);
        serial_write("[memory] memory map unavailable\n");
    }
    vfs_init();
    pkg_init();
    blockdev_init();
    fat32_init();
    emrefs_init();
    if (blockdev_is_ready()) {
        if (fat32_auto_mount()) serial_write("[fat32] mounted read-only volume\n");
        else serial_write("[fat32] no supported FAT32 partition found\n");
        if (emrefs_auto_mount()) serial_write("[emrefs] mounted custom partition\n");
        else serial_write("[emrefs] no EmreFS partition found\n");
    } else {
        serial_write("[block] no supported legacy ATA device; disk features unavailable\n");
    }

    if (mouse_init()) {
        serial_write("[mouse] PS/2 mouse initialized\n");
    } else {
        serial_write("[mouse] PS/2 initialization failed\n");
    }

    if (fb_available())
        draw_desktop();

    serial_write("[kernel] interactive desktop ready\n");

    for (;;) {
        /*
         * The mouse driver consumes one byte per poll.
         * It must check the controller status before reading.
         */
        if (mouse_poll()) {
            struct mouse_state mouse = mouse_get_state();
            int old_x = mouse_x, old_y = mouse_y;
            uint8_t old_buttons = previous_buttons;
            int was_dragging = dragging;

            handle_mouse_event(mouse);

            if (fb_available()) {
                int moved = old_x != mouse_x || old_y != mouse_y;
                int buttons_changed = old_buttons != mouse.buttons;
                if (moved && !buttons_changed && !was_dragging && !dragging) {
                    /* Cursor-only update: no full-screen clear/redraw. */
                    cursor_restore();
                    draw_cursor(mouse_x, mouse_y);
                } else if (moved && (was_dragging || dragging)) {
                    /* Repaint only the union of the old/new window rectangles.
                       This avoids full-screen flashing while moving windows. */
                    cursor_restore();
                    int x0 = window.x, y0 = window.y;
                    int x1 = window.x + window.w, y1 = window.y + window.h;
                    /* Include previous window location inferred from pointer delta. */
                    int dx = mouse_x - old_x, dy = mouse_y - old_y;
                    int ox0 = x0 - dx, oy0 = y0 - dy;
                    int ox1 = ox0 + window.w, oy1 = oy0 + window.h;
                    if (ox0 < x0) x0 = ox0;
                    if (oy0 < y0) y0 = oy0;
                    if (ox1 > x1) x1 = ox1;
                    if (oy1 > y1) y1 = oy1;
                    int pad = 9;
                    fb_set_clip(1, x0 - pad, y0 - pad, (x1 - x0) + pad * 2, (y1 - y0) + pad * 2);
                    draw_desktop();
                    fb_set_clip(0, 0, 0, 0, 0);
                } else if (moved || buttons_changed || was_dragging || dragging) {
                    cursor_restore();
                    draw_desktop();
                }
            }
        }

        int c = keyboard_read_char();

        if (c) {
            if (window.open && window.app_id == 3) {
                shell_key(c);
                if (fb_available()) draw_desktop();
            } else if (c == '\n') {
                serial_write("\n");
            } else if (c == '\b') {
                serial_write("[backspace]");
            } else {
                char output[2] = { (char)c, '\0' };
                serial_write(output);
            }
        }

        __asm__ volatile ("pause");
    }
}
