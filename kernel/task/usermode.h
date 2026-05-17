#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>
#include "arch.h"
#include "fs.h"
#include "net.h"
#include "syscall.h"
#include "user_tls.h"

#define USER_APP_STATUS_OK 0
#define USER_APP_STATUS_RUNNING 1
#define USER_KERNEL_RETURN_NONE 0U
#define USER_KERNEL_RETURN_KERNEL 1U

typedef struct process process_t;

enum {
    USER_SNAKE_SURFACE_W = 426,
    USER_SNAKE_SURFACE_H = 384,
    USER_NARCPAD_SURFACE_W = 881,
    USER_NARCPAD_SURFACE_H = 535,
    USER_SETTINGS_SURFACE_W = 520,
    USER_SETTINGS_SURFACE_H = 420,
    USER_EXPLORER_SURFACE_W = 1280,
    USER_EXPLORER_SURFACE_H = 960
};

typedef struct {
    int px[100];
    int py[100];
    int len;
    int apple_x;
    int apple_y;
    int dead;
    int score;
    int best;
    int dir;
    int last_tick;
    int render_w;
    int render_h;
    uint32_t* surface_ptr;
    uint32_t surface[USER_SNAKE_SURFACE_W * USER_SNAKE_SURFACE_H];
} user_snake_state_t;

typedef struct {
    int status;
    uint32_t use_https;
    uint32_t debug_stage;
    net_http_result_t result;
    char host[96];
    char path[160];
    char response[2048];
} user_netdemo_state_t;

typedef struct {
    int status;
    uint32_t use_https;
    uint32_t body_offset;
    uint32_t saved_len;
    net_http_result_t result;
    char host[96];
    char path[160];
    char output_path[128];
    char response[4096];
} user_fetch_state_t;

typedef struct {
    int status;
    int exit_code;
    net_http_result_t http_result;
    net_ping_result_t ping_result;
    user_tls_selftest_report_t tls_report;
    rtc_local_time_t local_time;
    disk_fs_node_t dir_entries[MAX_FILES];
    char command[128];
    char scratch[4096];
    char aux[4096];
} user_shell_state_t;

#define USER_GUI_EVENT_QUEUE_CAP 64

typedef enum {
    USER_NARCPAD_EVT_NONE = 0,
    USER_NARCPAD_EVT_CHAR,
    USER_NARCPAD_EVT_BACKSPACE,
    USER_NARCPAD_EVT_NEWLINE,
    USER_NARCPAD_EVT_SAVE,
    USER_NARCPAD_EVT_OPEN_NEW,
    USER_NARCPAD_EVT_OPEN_PATH,
    USER_NARCPAD_EVT_SCROLL
} user_narcpad_event_t;

typedef enum {
    USER_SETTINGS_EVT_NONE = 0,
    USER_SETTINGS_EVT_ADJUST_OFFSET,
    USER_SETTINGS_EVT_SET_OFFSET,
    USER_SETTINGS_EVT_OPEN_CONFIG,
    USER_SETTINGS_EVT_POINTER_DOWN,
    USER_SETTINGS_EVT_KEY_DOWN
} user_settings_event_t;

#define USER_SETTINGS_POINT_X_MASK 0xFFFF
#define USER_SETTINGS_POINT_Y_MASK 0xFFFF
#define USER_SETTINGS_PACK_POINT(x, y) \
    ((((int32_t)(y) & USER_SETTINGS_POINT_Y_MASK) << 16) | ((int32_t)(x) & USER_SETTINGS_POINT_X_MASK))
#define USER_SETTINGS_POINT_X(value) ((int16_t)((value) & USER_SETTINGS_POINT_X_MASK))
#define USER_SETTINGS_POINT_Y(value) ((int16_t)(((value) >> 16) & USER_SETTINGS_POINT_Y_MASK))

typedef enum {
    USER_EXPLORER_MODAL_NONE = 0,
    USER_EXPLORER_MODAL_RENAME,
    USER_EXPLORER_MODAL_DELETE
} user_explorer_modal_t;

typedef enum {
    USER_EXPLORER_EVT_NONE = 0,
    USER_EXPLORER_EVT_OPEN_DIR,
    USER_EXPLORER_EVT_SELECT_IDX,
    USER_EXPLORER_EVT_OPEN_SELECTED,
    USER_EXPLORER_EVT_CREATE_FILE,
    USER_EXPLORER_EVT_CREATE_DIR,
    USER_EXPLORER_EVT_BEGIN_RENAME,
    USER_EXPLORER_EVT_BEGIN_DELETE,
    USER_EXPLORER_EVT_MODAL_CHAR,
    USER_EXPLORER_EVT_MODAL_BACKSPACE,
    USER_EXPLORER_EVT_MODAL_SUBMIT,
    USER_EXPLORER_EVT_MODAL_CANCEL,
    USER_EXPLORER_EVT_MOVE_SELECTED_TO,
    USER_EXPLORER_EVT_GO_BACK,
    USER_EXPLORER_EVT_GO_UP,
    USER_EXPLORER_EVT_REFRESH,
    USER_EXPLORER_EVT_POINTER_DOWN,
    USER_EXPLORER_EVT_POINTER_UP,
    USER_EXPLORER_EVT_POINTER_DBLCLICK,
    USER_EXPLORER_EVT_POINTER_WHEEL
} user_explorer_event_t;

#define USER_EXPLORER_POINT_X_MASK 0xFFFF
#define USER_EXPLORER_POINT_Y_MASK 0xFFFF
#define USER_EXPLORER_PACK_POINT(x, y) \
    ((((int32_t)(y) & USER_EXPLORER_POINT_Y_MASK) << 16) | ((int32_t)(x) & USER_EXPLORER_POINT_X_MASK))
#define USER_EXPLORER_POINT_X(value) ((int16_t)((value) & USER_EXPLORER_POINT_X_MASK))
#define USER_EXPLORER_POINT_Y(value) ((int16_t)(((value) >> 16) & USER_EXPLORER_POINT_Y_MASK))

typedef struct {
    int status;
    int dirty;
    int last_blink_phase;
    int view_scroll;
    int render_w;
    int render_h;
    uint32_t* surface;
    int event_head;
    int event_tail;
    uint16_t event_type[USER_GUI_EVENT_QUEUE_CAP];
    int32_t event_arg[USER_GUI_EVENT_QUEUE_CAP];
    char title[32];
    char path[256];
    char request_path[256];
    char content[1024];
} user_narcpad_state_t;

typedef struct {
    int status;
    int dirty;
    int render_w;
    int render_h;
    uint32_t* surface;
    int event_head;
    int event_tail;
    uint16_t event_type[USER_GUI_EVENT_QUEUE_CAP];
    int32_t event_arg[USER_GUI_EVENT_QUEUE_CAP];
} user_settings_state_t;

typedef struct {
    int status;
    int dirty;
    int render_w;
    int render_h;
    int current_dir;
    int prev_dir;
    int selected_idx;
    int list_scroll;
    int drag_candidate_idx;
    int modal_mode;
    int modal_input_len;
    uint32_t* surface;
    int event_head;
    int event_tail;
    uint16_t event_type[USER_GUI_EVENT_QUEUE_CAP];
    int32_t event_arg[USER_GUI_EVENT_QUEUE_CAP];
    char modal_input[32];
} user_explorer_state_t;

int init_usermode();
void run_user_tasks();
void stop_all_background_user_tasks();
int run_user_netdemo(const char* target);
int run_user_https_command(const char* target);
int run_user_fetch(const char* args);
int run_user_shell_command(const char* command);
int usermode_prepare_process_context(process_t* proc);
int usermode_run_external_process(process_t* proc);
int usermode_schedule_current_process_exit(int exit_code);
uintptr_t usermode_active_trap_stack_top(void);
process_t* usermode_active_process(void);
void user_yield_handler(arch_trap_frame_t* frame);
void usermode_debug_dump(const char* tag);
int usermode_exit_current_task(int exit_code);

extern user_netdemo_state_t* user_netdemo_state_ptr;
extern user_fetch_state_t* user_fetch_state_ptr;
extern user_shell_state_t* user_shell_state_ptr;
#define user_netdemo_state  (*user_netdemo_state_ptr)
#define user_fetch_state    (*user_fetch_state_ptr)
#define user_shell_state    (*user_shell_state_ptr)
extern uintptr_t user_kernel_resume_esp;
extern uintptr_t user_kernel_ebx;
extern uintptr_t user_kernel_esi;
extern uintptr_t user_kernel_edi;
extern uintptr_t user_kernel_ebp;
extern uintptr_t user_kernel_return_mode;
extern arch_trap_frame_t* user_current_task_frame_ptr;

#endif
