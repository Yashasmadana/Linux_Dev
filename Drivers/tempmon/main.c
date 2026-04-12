#define _POSIX_C_SOURCE 200112L

/*
 ******************************************************************************
 * TempMon - ncurses TUI Temperature Monitor for BeagleBone Black
 *
 * Reads from LM75 kernel driver (sysfs or /dev) and displays:
 *   - Live temperature with large readout
 *   - Scrolling 60-sample history graph
 *   - CPU load bar, memory bar, CPU temperature
 *   - Configurable overtemp alert
 *
 * Build (Linux/WSL):  make
 * Build (Windows):    make win
 * Run:                ./tempmon
 * Cross (BBB):        make cross
 *
 * License: GPL v2
 ******************************************************************************
 */

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/utsname.h>
#  include <signal.h>
#endif

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

#define APP_NAME        "TempMon"
#define APP_VERSION     "1.0"
#define GRAPH_SAMPLES   60
#define DEFAULT_ALERT   60.0f
#define ALERT_STEP      1.0f
#define SIM_FALLBACK    1

/* LM75 sysfs paths (Linux only) */
static const char *LM75_SYSFS_PATHS[] = {
    "/sys/class/lm75_temp_sensor/lm75_temp_sensor/temperature",
    "/sys/class/hwmon/hwmon0/temp1_input",
    "/sys/class/hwmon/hwmon1/temp1_input",
    "/sys/class/hwmon/hwmon2/temp1_input",
    NULL
};

#define LM75_DEV_PATH   "/dev/lm75_temp_sensor"
#define CPU_TEMP_PATH   "/sys/class/thermal/thermal_zone0/temp"

/* ============================================================
 * COLOR PAIRS
 * ============================================================ */

#define CLR_TITLE       1
#define CLR_BORDER      2
#define CLR_TEMP_NORMAL 3
#define CLR_TEMP_WARN   4
#define CLR_TEMP_ALERT  5
#define CLR_LABEL       7
#define CLR_STATUSBAR   8

/* ============================================================
 * DATA STRUCTURES
 * ============================================================ */

typedef struct {
    float   samples[GRAPH_SAMPLES];
    int     head;
    int     count;
} TempHistory;

#ifndef _WIN32
typedef struct {
    unsigned long user, nice, system, idle, iowait, irq, softirq;
} CpuStat;
#endif

typedef struct {
    float       lm75_temp;
    float       cpu_temp;
    float       alert_threshold;
    int         alert_active;
    TempHistory history;

    float       cpu_load;
    float       mem_used_pct;
    long        mem_total_mb;
    long        mem_used_mb;
    long        uptime_sec;
    unsigned long sample_count;

    int         paused;
    int         quit;

#ifndef _WIN32
    CpuStat     prev_cpu;
#else
    ULONGLONG   prev_idle;
    ULONGLONG   prev_kernel;
    ULONGLONG   prev_user;
#endif

    char        lm75_path[256];
    int         lm75_millideg;
    int         simulated;
    float       sim_phase;
} AppState;

/* ============================================================
 * SENSOR READING
 * ============================================================ */

static int probe_lm75(AppState *s)
{
#ifdef _WIN32
    /* No real sensor on Windows — always simulate */
    (void)s;
    return -1;
#else
    FILE *f;
    int i;

    for (i = 0; LM75_SYSFS_PATHS[i]; i++) {
        f = fopen(LM75_SYSFS_PATHS[i], "r");
        if (!f) continue;
        float raw = 0;
        int ok = (fscanf(f, "%f", &raw) == 1);
        fclose(f);
        if (ok) {
            strncpy(s->lm75_path, LM75_SYSFS_PATHS[i], sizeof(s->lm75_path) - 1);
            s->lm75_millideg = (strstr(LM75_SYSFS_PATHS[i], "hwmon") != NULL);
            return 0;
        }
    }

    f = fopen(LM75_DEV_PATH, "r");
    if (f) {
        float raw = 0;
        int ok = (fscanf(f, "Temperature: %f", &raw) == 1);
        fclose(f);
        if (ok) {
            strncpy(s->lm75_path, LM75_DEV_PATH, sizeof(s->lm75_path) - 1);
            s->lm75_millideg = 0;
            return 0;
        }
    }

    return -1;
#endif
}

static float read_lm75(AppState *s)
{
    if (s->simulated) {
        s->sim_phase += 0.05f;
        return 35.0f + 10.0f * sinf(s->sim_phase);
    }

#ifdef _WIN32
    return -999.0f;
#else
    FILE *f = fopen(s->lm75_path, "r");
    if (!f) return -999.0f;

    float raw = 0;
    if (strstr(s->lm75_path, LM75_DEV_PATH))
        fscanf(f, "Temperature: %f", &raw);
    else {
        fscanf(f, "%f", &raw);
        if (s->lm75_millideg) raw /= 1000.0f;
    }
    fclose(f);
    return raw;
#endif
}

static float read_cpu_temp(void)
{
#ifdef _WIN32
    return -1.0f;
#else
    FILE *f = fopen(CPU_TEMP_PATH, "r");
    if (!f) return -1.0f;
    int raw = 0;
    fscanf(f, "%d", &raw);
    fclose(f);
    return raw / 1000.0f;
#endif
}

static float read_cpu_load(AppState *s)
{
#ifdef _WIN32
    FILETIME ft_idle, ft_kernel, ft_user;
    if (!GetSystemTimes(&ft_idle, &ft_kernel, &ft_user))
        return 0.0f;

    ULONGLONG idle   = ((ULONGLONG)ft_idle.dwHighDateTime   << 32) | ft_idle.dwLowDateTime;
    ULONGLONG kernel = ((ULONGLONG)ft_kernel.dwHighDateTime << 32) | ft_kernel.dwLowDateTime;
    ULONGLONG user   = ((ULONGLONG)ft_user.dwHighDateTime   << 32) | ft_user.dwLowDateTime;

    ULONGLONG d_idle   = idle   - s->prev_idle;
    ULONGLONG d_kernel = kernel - s->prev_kernel;
    ULONGLONG d_user   = user   - s->prev_user;

    s->prev_idle   = idle;
    s->prev_kernel = kernel;
    s->prev_user   = user;

    ULONGLONG total = d_kernel + d_user;
    ULONGLONG busy  = total - d_idle;

    if (total == 0) return 0.0f;
    return (float)busy / (float)total * 100.0f;
#else
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0.0f;

    CpuStat cur = {0};
    unsigned long steal = 0, guest = 0;
    fscanf(f, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
           &cur.user, &cur.nice, &cur.system, &cur.idle,
           &cur.iowait, &cur.irq, &cur.softirq, &steal, &guest);
    fclose(f);

    CpuStat *p = &s->prev_cpu;
    unsigned long d_user    = cur.user    - p->user;
    unsigned long d_nice    = cur.nice    - p->nice;
    unsigned long d_sys     = cur.system  - p->system;
    unsigned long d_idle    = cur.idle    - p->idle;
    unsigned long d_iowait  = cur.iowait  - p->iowait;
    unsigned long d_irq     = cur.irq     - p->irq;
    unsigned long d_softirq = cur.softirq - p->softirq;
    unsigned long total = d_user + d_nice + d_sys + d_idle + d_iowait + d_irq + d_softirq;
    unsigned long busy  = total - d_idle - d_iowait;
    *p = cur;
    if (total == 0) return 0.0f;
    return (float)busy / (float)total * 100.0f;
#endif
}

static void read_memory(AppState *s)
{
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return;
    s->mem_total_mb = (long)(ms.ullTotalPhys / (1024 * 1024));
    s->mem_used_mb  = (long)((ms.ullTotalPhys - ms.ullAvailPhys) / (1024 * 1024));
    s->mem_used_pct = (float)ms.dwMemoryLoad;
#else
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    long mem_total = 0, mem_free = 0, mem_buffers = 0, mem_cached = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        long val;
        if      (sscanf(line, "MemTotal: %ld kB",  &val) == 1) mem_total   = val;
        else if (sscanf(line, "MemFree: %ld kB",   &val) == 1) mem_free    = val;
        else if (sscanf(line, "Buffers: %ld kB",   &val) == 1) mem_buffers = val;
        else if (sscanf(line, "Cached: %ld kB",    &val) == 1) mem_cached  = val;
    }
    fclose(f);
    long used = mem_total - mem_free - mem_buffers - mem_cached;
    s->mem_total_mb = mem_total / 1024;
    s->mem_used_mb  = (used > 0) ? used / 1024 : 0;
    s->mem_used_pct = (mem_total > 0) ? (float)used / (float)mem_total * 100.0f : 0.0f;
    if (s->mem_used_pct < 0) s->mem_used_pct = 0.0f;
#endif
}

static long read_uptime(void)
{
#ifdef _WIN32
    return (long)(GetTickCount64() / 1000);
#else
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return 0;
    double up = 0;
    fscanf(f, "%lf", &up);
    fclose(f);
    return (long)up;
#endif
}

/* ============================================================
 * HISTORY BUFFER
 * ============================================================ */

static void history_push(TempHistory *h, float temp)
{
    h->samples[h->head] = temp;
    h->head = (h->head + 1) % GRAPH_SAMPLES;
    if (h->count < GRAPH_SAMPLES) h->count++;
}

static float history_get(const TempHistory *h, int age)
{
    int idx = ((h->head - 1 - age) + GRAPH_SAMPLES) % GRAPH_SAMPLES;
    return h->samples[idx];
}

/* ============================================================
 * DRAWING HELPERS
 * ============================================================ */

static void draw_hline(int y, int x, int len, chtype ch)
{
    for (int i = 0; i < len; i++) mvaddch(y, x + i, ch);
}

static void draw_bar(int y, int x, int width, float pct,
                     float warn_at, float crit_at)
{
    int inner = width - 2;
    int fill  = (int)(pct / 100.0f * inner + 0.5f);
    if (fill < 0) fill = 0;
    if (fill > inner) fill = inner;

    int color = CLR_TEMP_NORMAL;
    if (pct >= crit_at) color = CLR_TEMP_ALERT;
    else if (pct >= warn_at) color = CLR_TEMP_WARN;

    mvaddch(y, x, '[');
    attron(COLOR_PAIR(color));
    for (int i = 0; i < fill; i++)   mvaddch(y, x + 1 + i, '#');
    attroff(COLOR_PAIR(color));
    attron(COLOR_PAIR(CLR_LABEL));
    for (int i = fill; i < inner; i++) mvaddch(y, x + 1 + i, '.');
    attroff(COLOR_PAIR(CLR_LABEL));
    mvaddch(y, x + 1 + inner, ']');
}

/* ============================================================
 * HOSTNAME HELPER
 * ============================================================ */

static void get_hostname(char *buf, int len)
{
#ifdef _WIN32
    DWORD sz = (DWORD)len;
    if (!GetComputerNameA(buf, &sz))
        strncpy(buf, "Windows", len - 1);
#else
    struct utsname uts;
    uname(&uts);
    strncpy(buf, uts.nodename, len - 1);
#endif
    buf[len - 1] = '\0';
}

/* ============================================================
 * MAIN UI DRAW
 * ============================================================ */

static void draw_ui(AppState *s)
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    if (rows < 20 || cols < 60) {
        clear();
        mvprintw(0, 0, "Terminal too small! Need at least 60x20.");
        refresh();
        return;
    }

    erase();

    /* Title bar */
    attron(COLOR_PAIR(CLR_TITLE) | A_BOLD);
    for (int i = 0; i < cols; i++) mvaddch(0, i, ' ');

    char hostname[64];
    get_hostname(hostname, sizeof(hostname));

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timebuf[16];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_info);

    char title[128];
    snprintf(title, sizeof(title), " %s v%s  |  %s  |  %s ",
             APP_NAME, APP_VERSION, hostname, timebuf);
    mvprintw(0, (cols - (int)strlen(title)) / 2, "%s", title);
    attroff(COLOR_PAIR(CLR_TITLE) | A_BOLD);

    /* Layout */
    int panel_h   = 9;
    int left_w    = cols / 2;
    int right_w   = cols - left_w;
    int graph_top = panel_h + 1;
    int graph_h   = rows - graph_top - 2;
    if (graph_h < 6) graph_h = 6;

    /* Left panel border */
    attron(COLOR_PAIR(CLR_BORDER));
    mvprintw(1, 0, "+"); draw_hline(1, 1, left_w - 2, '-'); mvprintw(1, left_w - 1, "+");
    for (int r = 2; r < panel_h; r++) { mvaddch(r, 0, '|'); mvaddch(r, left_w - 1, '|'); }
    mvprintw(panel_h, 0, "+"); draw_hline(panel_h, 1, left_w - 2, '-'); mvprintw(panel_h, left_w - 1, "+");
    attroff(COLOR_PAIR(CLR_BORDER));

    attron(COLOR_PAIR(CLR_LABEL) | A_BOLD);
    mvprintw(1, 2, " LM75 AMBIENT ");
    attroff(COLOR_PAIR(CLR_LABEL) | A_BOLD);

    if (s->simulated) {
        attron(COLOR_PAIR(CLR_TEMP_WARN));
        mvprintw(2, 2, "[SIMULATED - no sensor]");
        attroff(COLOR_PAIR(CLR_TEMP_WARN));
    } else {
        attron(COLOR_PAIR(CLR_LABEL));
        int maxpath = left_w - 5;
        char pathbuf[256];
        strncpy(pathbuf, s->lm75_path, sizeof(pathbuf) - 1);
        if ((int)strlen(pathbuf) > maxpath) {
            pathbuf[maxpath - 3] = '.'; pathbuf[maxpath - 2] = '.';
            pathbuf[maxpath - 1] = '.'; pathbuf[maxpath] = '\0';
        }
        mvprintw(2, 2, "src: %s", pathbuf);
        attroff(COLOR_PAIR(CLR_LABEL));
    }

    int temp_color = CLR_TEMP_NORMAL;
    if (s->lm75_temp >= s->alert_threshold) temp_color = CLR_TEMP_ALERT;
    else if (s->lm75_temp >= s->alert_threshold - 10.0f) temp_color = CLR_TEMP_WARN;

    attron(COLOR_PAIR(temp_color) | A_BOLD);
    char tempbuf[32];
    snprintf(tempbuf, sizeof(tempbuf), ">>> %6.1f C <<<", s->lm75_temp);
    mvprintw(4, (left_w - (int)strlen(tempbuf)) / 2, "%s", tempbuf);
    attroff(COLOR_PAIR(temp_color) | A_BOLD);

    attron(COLOR_PAIR(CLR_LABEL));
    mvprintw(6, 2, "Alert threshold: %.1f C", s->alert_threshold);
    attroff(COLOR_PAIR(CLR_LABEL));

    if (s->alert_active) {
        attron(COLOR_PAIR(CLR_TEMP_ALERT) | A_BOLD | A_BLINK);
        mvprintw(7, 2, "!!! OVERTEMP ALERT !!!");
        attroff(COLOR_PAIR(CLR_TEMP_ALERT) | A_BOLD | A_BLINK);
    } else {
        attron(COLOR_PAIR(CLR_TEMP_NORMAL));
        mvprintw(7, 2, "Status: NORMAL");
        attroff(COLOR_PAIR(CLR_TEMP_NORMAL));
    }

    /* Right panel border */
    attron(COLOR_PAIR(CLR_BORDER));
    mvprintw(1, left_w, "+"); draw_hline(1, left_w + 1, right_w - 2, '-'); mvprintw(1, cols - 1, "+");
    for (int r = 2; r < panel_h; r++) { mvaddch(r, left_w, '|'); mvaddch(r, cols - 1, '|'); }
    mvprintw(panel_h, left_w, "+"); draw_hline(panel_h, left_w + 1, right_w - 2, '-'); mvprintw(panel_h, cols - 1, "+");
    attroff(COLOR_PAIR(CLR_BORDER));

    attron(COLOR_PAIR(CLR_LABEL) | A_BOLD);
    mvprintw(1, left_w + 2, " SYSTEM STATUS ");
    attroff(COLOR_PAIR(CLR_LABEL) | A_BOLD);

    int rx    = left_w + 2;
    int bar_w = right_w - 14;

    attron(COLOR_PAIR(CLR_LABEL));
    mvprintw(2, rx, "CPU Load: %5.1f%%", s->cpu_load);
    attroff(COLOR_PAIR(CLR_LABEL));
    draw_bar(3, rx, bar_w, s->cpu_load, 60.0f, 85.0f);

    attron(COLOR_PAIR(CLR_LABEL));
    mvprintw(4, rx, "Memory:   %5.1f%%  (%ld/%ld MB)",
             s->mem_used_pct, s->mem_used_mb, s->mem_total_mb);
    attroff(COLOR_PAIR(CLR_LABEL));
    draw_bar(5, rx, bar_w, s->mem_used_pct, 70.0f, 90.0f);

    attron(COLOR_PAIR(CLR_LABEL));
    if (s->cpu_temp > 0)
        mvprintw(6, rx, "CPU Temp: %.1f C", s->cpu_temp);
    else
        mvprintw(6, rx, "CPU Temp: N/A");
    attroff(COLOR_PAIR(CLR_LABEL));

    long up = s->uptime_sec;
    long d = up / 86400, h = (up % 86400) / 3600, m = (up % 3600) / 60;
    attron(COLOR_PAIR(CLR_LABEL));
    mvprintw(7, rx, "Uptime: %ldd %ldh %ldm  Samples: %lu", d, h, m, s->sample_count);
    attroff(COLOR_PAIR(CLR_LABEL));

    /* Graph section */
    attron(COLOR_PAIR(CLR_BORDER));
    mvprintw(graph_top, 0, "+");
    {
        char ghead[64];
        snprintf(ghead, sizeof(ghead), "---- GRAPH (%d samples @ 1s) ", GRAPH_SAMPLES);
        mvprintw(graph_top, 1, "%s", ghead);
        draw_hline(graph_top, 1 + (int)strlen(ghead), cols - 2 - (int)strlen(ghead), '-');
    }
    mvprintw(graph_top, cols - 1, "+");
    for (int r = graph_top + 1; r < graph_top + graph_h + 1; r++) {
        mvaddch(r, 0, '|');
        mvaddch(r, cols - 1, '|');
    }
    mvprintw(graph_top + graph_h + 1, 0, "+");
    draw_hline(graph_top + graph_h + 1, 1, cols - 2, '-');
    mvprintw(graph_top + graph_h + 1, cols - 1, "+");
    attroff(COLOR_PAIR(CLR_BORDER));

    /* Auto-scale Y */
    float y_min = 10.0f, y_max = 80.0f;
    if (s->history.count > 0) {
        float hmin = 9999, hmax = -9999;
        for (int i = 0; i < s->history.count; i++) {
            float v = history_get(&s->history, i);
            if (v < hmin) hmin = v;
            if (v > hmax) hmax = v;
        }
        float margin = (hmax - hmin) * 0.2f;
        if (margin < 5.0f) margin = 5.0f;
        y_min = floorf(hmin - margin);
        y_max = ceilf(hmax + margin);
    }

    int gx_start = 6;
    int gx_end   = cols - 2;
    int gw       = gx_end - gx_start;
    int gy_start = graph_top + 1;

    /* Y labels + alert dashed line */
    for (int row = 0; row < graph_h; row++) {
        float temp_at = y_max - (float)row / (float)(graph_h - 1) * (y_max - y_min);
        attron(COLOR_PAIR(CLR_LABEL));
        mvprintw(gy_start + row, 1, "%4.0f|", temp_at);
        attroff(COLOR_PAIR(CLR_LABEL));
        if (fabsf(temp_at - s->alert_threshold) < (y_max - y_min) / graph_h) {
            attron(COLOR_PAIR(CLR_TEMP_ALERT));
            for (int c = gx_start; c < gx_end; c++) mvaddch(gy_start + row, c, '-');
            attroff(COLOR_PAIR(CLR_TEMP_ALERT));
        }
    }

    /* Plot */
    int n = (s->history.count < gw) ? s->history.count : gw;
    for (int i = 0; i < n; i++) {
        float v   = history_get(&s->history, n - 1 - i);
        int   col = gx_start + (gw - n) + i;
        float norm = (v - y_min) / (y_max - y_min);
        if (norm < 0) norm = 0;
        if (norm > 1) norm = 1;
        int row = gy_start + (int)((1.0f - norm) * (graph_h - 1));

        int pc = CLR_TEMP_NORMAL;
        if (v >= s->alert_threshold) pc = CLR_TEMP_ALERT;
        else if (v >= s->alert_threshold - 10.0f) pc = CLR_TEMP_WARN;

        attron(COLOR_PAIR(pc) | A_BOLD);
        mvaddch(row, col, '*');
        attroff(COLOR_PAIR(pc) | A_BOLD);
        attron(COLOR_PAIR(pc));
        for (int r = row + 1; r < gy_start + graph_h; r++) mvaddch(r, col, '|');
        attroff(COLOR_PAIR(pc));
    }

    if (s->paused) {
        attron(COLOR_PAIR(CLR_TEMP_WARN) | A_BOLD);
        mvprintw(gy_start + graph_h / 2, (cols - 9) / 2, "[ PAUSED ]");
        attroff(COLOR_PAIR(CLR_TEMP_WARN) | A_BOLD);
    }

    /* Status bar */
    attron(COLOR_PAIR(CLR_STATUSBAR));
    for (int i = 0; i < cols; i++) mvaddch(rows - 1, i, ' ');
    mvprintw(rows - 1, 1,
             " [q]Quit  [+]Alert+  [-]Alert-  [p]Pause  [r]Reset  [s]Sensor info ");
    attroff(COLOR_PAIR(CLR_STATUSBAR));

    refresh();
}

/* ============================================================
 * SIGNAL HANDLER (Linux only)
 * ============================================================ */

#ifndef _WIN32
static volatile int g_resize = 0;
static void on_sigwinch(int sig) { (void)sig; g_resize = 1; }
#endif

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    AppState s;
    memset(&s, 0, sizeof(s));
    s.alert_threshold = DEFAULT_ALERT;

    if (probe_lm75(&s) < 0) {
        if (SIM_FALLBACK) {
            s.simulated = 1;
            strncpy(s.lm75_path, "(simulated)", sizeof(s.lm75_path) - 1);
        } else {
            fprintf(stderr, "Error: LM75 sensor not found.\n");
            return 1;
        }
    }

#ifndef _WIN32
    /* Seed CPU baseline */
    {
        FILE *f = fopen("/proc/stat", "r");
        if (f) {
            unsigned long steal = 0, guest = 0;
            fscanf(f, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &s.prev_cpu.user, &s.prev_cpu.nice, &s.prev_cpu.system,
                   &s.prev_cpu.idle, &s.prev_cpu.iowait, &s.prev_cpu.irq,
                   &s.prev_cpu.softirq, &steal, &guest);
            fclose(f);
        }
    }
#else
    /* Seed Windows CPU baseline */
    {
        FILETIME fi, fk, fu;
        if (GetSystemTimes(&fi, &fk, &fu)) {
            s.prev_idle   = ((ULONGLONG)fi.dwHighDateTime << 32) | fi.dwLowDateTime;
            s.prev_kernel = ((ULONGLONG)fk.dwHighDateTime << 32) | fk.dwLowDateTime;
            s.prev_user   = ((ULONGLONG)fu.dwHighDateTime << 32) | fu.dwLowDateTime;
        }
    }
#endif

    /* ncurses init */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CLR_TITLE,       COLOR_BLACK,  COLOR_CYAN);
        init_pair(CLR_BORDER,      COLOR_CYAN,   -1);
        init_pair(CLR_TEMP_NORMAL, COLOR_GREEN,  -1);
        init_pair(CLR_TEMP_WARN,   COLOR_YELLOW, -1);
        init_pair(CLR_TEMP_ALERT,  COLOR_RED,    -1);
        init_pair(CLR_LABEL,       COLOR_WHITE,  -1);
        init_pair(CLR_STATUSBAR,   COLOR_BLACK,  COLOR_WHITE);
    }

#ifndef _WIN32
    signal(SIGWINCH, on_sigwinch);
#endif

    /* Initial sample */
    s.lm75_temp  = read_lm75(&s);
    s.cpu_temp   = read_cpu_temp();
    s.cpu_load   = read_cpu_load(&s);
    s.uptime_sec = read_uptime();
    read_memory(&s);
    history_push(&s.history, s.lm75_temp);
    s.sample_count = 1;

    time_t last_poll = time(NULL);

    while (!s.quit) {
#ifndef _WIN32
        if (g_resize) {
            g_resize = 0;
            endwin(); refresh(); clear();
        }
#endif
        /* Input */
        int ch = getch();
        switch (ch) {
        case 'q': case 'Q': case 27:
            s.quit = 1; break;
        case '+': case '=':
            s.alert_threshold += ALERT_STEP; break;
        case '-': case '_':
            s.alert_threshold -= ALERT_STEP;
            if (s.alert_threshold < 1.0f) s.alert_threshold = 1.0f;
            break;
        case 'p': case 'P':
            s.paused = !s.paused; break;
        case 'r': case 'R':
            memset(&s.history, 0, sizeof(s.history));
            s.sample_count = 0; break;
        case 's': case 'S': {
            int rows, cols;
            getmaxyx(stdscr, rows, cols);
            attron(COLOR_PAIR(CLR_TITLE) | A_BOLD);
            int px = cols / 4, py = rows / 3;
            mvprintw(py,     px, "+------ Sensor Info -------+");
            mvprintw(py + 1, px, "| Path: %-20s|", s.lm75_path);
            mvprintw(py + 2, px, "| Mode: %-20s|",
                     s.simulated ? "Simulated" :
                     (s.lm75_millideg ? "hwmon (mdeg)" : "sysfs (deg)"));
            mvprintw(py + 3, px, "|   Press any key to close  |");
            mvprintw(py + 4, px, "+---------------------------+");
            attroff(COLOR_PAIR(CLR_TITLE) | A_BOLD);
            refresh();
            nodelay(stdscr, FALSE);
            getch();
            nodelay(stdscr, TRUE);
            break;
        }
        default: break;
        }

        if (s.quit) break;

        /* Poll every second */
        time_t now = time(NULL);
        if (now != last_poll && !s.paused) {
            last_poll = now;
            s.lm75_temp  = read_lm75(&s);
            s.cpu_temp   = read_cpu_temp();
            s.cpu_load   = read_cpu_load(&s);
            s.uptime_sec = read_uptime();
            read_memory(&s);
            history_push(&s.history, s.lm75_temp);
            s.sample_count++;
            s.alert_active = (s.lm75_temp >= s.alert_threshold);
        }

        draw_ui(&s);
        napms(50);  /* portable 50ms sleep — avoids busy-spin */
    }

    endwin();

    if (s.simulated)
        printf("\nNote: no LM75 found — ran in simulated mode.\n");

    return 0;
}
