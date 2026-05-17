#include "usermode.h"
#include "paging.h"
#include "process.h"
#include "serial.h"
#include "string.h"
#include "vbe.h"
#include "mouse.h"
#include "rtc.h"

#define USERMODE_TRACE_DISPATCH 0

extern uint8_t __user_region_start[];
extern uint8_t __user_region_end[];
extern volatile int gui_needs_redraw;
extern volatile uint32_t timer_ticks;
extern void user_netdemo_entry_gate();
extern void user_fetch_entry_gate();
extern void user_shell_entry_gate();
extern void vga_print(const char* str);
extern void vga_println(const char* str);
extern void vga_print_color(const char* str, uint8_t color);
extern void vga_print_int(int num);
extern void net_print_ip(uint32_t ip);
extern int screen_is_graphics_enabled(void);
extern void vbe_compose_scene_basic(void);

typedef enum {
    USER_TASK_NONE = 0,
    USER_TASK_NETDEMO,
    USER_TASK_FETCH,
    USER_TASK_SHELL,
    USER_TASK_PROCESS
} user_task_kind_t;

typedef struct {
    user_task_kind_t active_user_task;
    process_t* active_external_process;
    arch_trap_frame_t* current_task_frame_ptr;
    uintptr_t kernel_resume_esp;
    uintptr_t kernel_ebx;
    uintptr_t kernel_esi;
    uintptr_t kernel_edi;
    uintptr_t kernel_ebp;
    uintptr_t kernel_return_mode;
} user_runtime_state_t;

static const char* user_https_stage_name(uint32_t stage) {
    switch (stage) {
        case 1U: return "entry";
        case 2U: return "fetch";
        default: return "unknown";
    }
}

static const char* user_task_kind_name(user_task_kind_t kind) {
    switch (kind) {
        case USER_TASK_NETDEMO: return "netdemo";
        case USER_TASK_FETCH: return "fetch";
        case USER_TASK_SHELL: return "shell";
        case USER_TASK_PROCESS: return "process";
        default: return "unknown";
    }
}

static void serial_write_hex_uintptr(uintptr_t value) {
#if UINTPTR_MAX > 0xFFFFFFFFU
    serial_write_hex64((uint64_t)value);
#else
    serial_write_hex32((uint32_t)value);
#endif
}

static arch_trap_frame_t netdemo_context;
static arch_trap_frame_t fetch_context;
static arch_trap_frame_t shell_context;
static user_task_kind_t active_user_task = USER_TASK_NONE;
static process_t* active_external_process = (process_t*)0;

static int init_user_memory_layout(void);

#define USER_PAGE_SIZE              4096U
#define USER_KERNEL_TRAP_STACK_PAGES 8U
#define USER_NETDEMO_STATE_PAGES    1U
#define USER_NETDEMO_STACK_PAGES    2U
#define USER_FETCH_STATE_PAGES      2U
#define USER_FETCH_STACK_PAGES      2U
#define USER_SHELL_STATE_PAGES      6U
#define USER_SHELL_STACK_PAGES      4U
#define USER_NETDEMO_STATE_VA       (USER_DATA_WINDOW_BASE + 0xA2000U)
#define USER_NETDEMO_STACK_VA       (USER_DATA_WINDOW_BASE + 0xA3000U)
#define USER_FETCH_STATE_VA         (USER_DATA_WINDOW_BASE + 0xA5000U)
#define USER_FETCH_STACK_VA         (USER_DATA_WINDOW_BASE + 0xA7000U)
#define USER_SHELL_STATE_VA         (USER_DATA_WINDOW_BASE + 0xA9000U)
#define USER_SHELL_STACK_VA         (USER_DATA_WINDOW_BASE + 0xAF000U)

static uint8_t* netdemo_state_region = (uint8_t*)0;
static uint8_t* netdemo_stack_region = (uint8_t*)0;
static uint8_t* netdemo_trap_stack_region = (uint8_t*)0;
static uint8_t* fetch_state_region = (uint8_t*)0;
static uint8_t* fetch_stack_region = (uint8_t*)0;
static uint8_t* fetch_trap_stack_region = (uint8_t*)0;
static uint8_t* shell_state_region = (uint8_t*)0;
static uint8_t* shell_stack_region = (uint8_t*)0;
static uint8_t* shell_trap_stack_region = (uint8_t*)0;
static int user_memory_ready = 0;
arch_trap_frame_t* user_current_task_frame_ptr = (arch_trap_frame_t*)0;

user_netdemo_state_t* user_netdemo_state_ptr = (user_netdemo_state_t*)0;
user_fetch_state_t* user_fetch_state_ptr = (user_fetch_state_t*)0;
user_shell_state_t* user_shell_state_ptr = (user_shell_state_t*)0;
uintptr_t user_kernel_resume_esp = 0;
uintptr_t user_kernel_ebx = 0;
uintptr_t user_kernel_esi = 0;
uintptr_t user_kernel_edi = 0;
uintptr_t user_kernel_ebp = 0;
uintptr_t user_kernel_return_mode = USER_KERNEL_RETURN_NONE;

static void save_user_runtime_state(user_runtime_state_t* state) {
    if (!state) return;
    state->active_user_task = active_user_task;
    state->active_external_process = active_external_process;
    state->current_task_frame_ptr = user_current_task_frame_ptr;
    state->kernel_resume_esp = user_kernel_resume_esp;
    state->kernel_ebx = user_kernel_ebx;
    state->kernel_esi = user_kernel_esi;
    state->kernel_edi = user_kernel_edi;
    state->kernel_ebp = user_kernel_ebp;
    state->kernel_return_mode = user_kernel_return_mode;
}

static int ensure_user_region(uint8_t** region_ptr, size_t page_count) {
    size_t size;

    if (!region_ptr || page_count == 0U) return -1;
    if (*region_ptr) return 0;

    *region_ptr = (uint8_t*)alloc_physical_pages(page_count);
    if (!*region_ptr) return -1;

    size = page_count * USER_PAGE_SIZE;
    memset(*region_ptr, 0, size);
    return 0;
}

static void restore_user_runtime_state(const user_runtime_state_t* state) {
    if (!state) return;
    active_user_task = state->active_user_task;
    active_external_process = state->active_external_process;
    user_current_task_frame_ptr = state->current_task_frame_ptr;
    user_kernel_resume_esp = state->kernel_resume_esp;
    user_kernel_ebx = state->kernel_ebx;
    user_kernel_esi = state->kernel_esi;
    user_kernel_edi = state->kernel_edi;
    user_kernel_ebp = state->kernel_ebp;
    user_kernel_return_mode = state->kernel_return_mode;
}

static int activate_builtin_user_space(void) {
    if (init_user_memory_layout() != 0) return -1;

    exec_deactivate_address_space();

    if (paging_map_user_region(USER_NETDEMO_STATE_VA, (uintptr_t)netdemo_state_region,
                               USER_NETDEMO_STATE_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_NETDEMO_STACK_VA, (uintptr_t)netdemo_stack_region,
                               USER_NETDEMO_STACK_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_FETCH_STATE_VA, (uintptr_t)fetch_state_region,
                               USER_FETCH_STATE_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_FETCH_STACK_VA, (uintptr_t)fetch_stack_region,
                               USER_FETCH_STACK_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_SHELL_STATE_VA, (uintptr_t)shell_state_region,
                               USER_SHELL_STATE_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_SHELL_STACK_VA, (uintptr_t)shell_stack_region,
                               USER_SHELL_STACK_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    return 0;
}

static uintptr_t trap_stack_region_top(uint8_t* region, uint32_t size) {
    return region ? (uintptr_t)region + size : (uintptr_t)KERNEL_BOOT_STACK_TOP;
}

static uintptr_t user_task_trap_stack_top(user_task_kind_t kind) {
    switch (kind) {
        case USER_TASK_NETDEMO:
            return trap_stack_region_top(netdemo_trap_stack_region, USER_KERNEL_TRAP_STACK_PAGES * USER_PAGE_SIZE);
        case USER_TASK_FETCH:
            return trap_stack_region_top(fetch_trap_stack_region, USER_KERNEL_TRAP_STACK_PAGES * USER_PAGE_SIZE);
        case USER_TASK_SHELL:
            return trap_stack_region_top(shell_trap_stack_region, USER_KERNEL_TRAP_STACK_PAGES * USER_PAGE_SIZE);
        case USER_TASK_PROCESS:
            if (active_external_process && active_external_process->arch.user_trap_stack_top != 0U) {
                return active_external_process->arch.user_trap_stack_top;
            }
            return (uintptr_t)KERNEL_BOOT_STACK_TOP;
        default:
            return (uintptr_t)KERNEL_BOOT_STACK_TOP;
    }
}

uintptr_t usermode_active_trap_stack_top(void) {
    return user_task_trap_stack_top(active_user_task);
}

process_t* usermode_active_process(void) {
    if (active_user_task == USER_TASK_PROCESS && active_external_process) return active_external_process;
    return 0;
}

static int user_context_in_user_code(uintptr_t ip) {
    uintptr_t start = (uintptr_t)__user_region_start;
    uintptr_t end = (uintptr_t)__user_region_end;
    return ip >= start && ip < end;
}

static int user_context_on_stack(uintptr_t sp, uintptr_t stack_base, uint32_t stack_pages) {
    uintptr_t stack_top = stack_base + (uintptr_t)stack_pages * USER_PAGE_SIZE;
    return sp >= stack_base && sp <= stack_top;
}

static int validate_user_context(arch_trap_frame_t* context, uintptr_t stack_base, uint32_t stack_pages) {
    if (!context) return 0;
    if (!user_context_in_user_code(arch_frame_user_ip(context))) return 0;
    if (!user_context_on_stack(arch_frame_user_sp(context), stack_base, stack_pages)) return 0;
    return 1;
}

static int validate_process_context(process_t* proc) {
    exec_image_t image;

    if (!proc) return 0;
    if (exec_query_image(&proc->user_space, &image) != EXEC_OK) return 0;
    if (arch_frame_user_ip(&proc->arch.user_frame) < image.image_base ||
        arch_frame_user_ip(&proc->arch.user_frame) >= image.image_limit) {
        return 0;
    }
    if (arch_frame_user_sp(&proc->arch.user_frame) < image.stack_base ||
        arch_frame_user_sp(&proc->arch.user_frame) > image.stack_top) {
        return 0;
    }
    return 1;
}

static void invalidate_user_task(user_task_kind_t kind) {
    if (kind == USER_TASK_NETDEMO) user_netdemo_state.status = -1;
    else if (kind == USER_TASK_FETCH) user_fetch_state.status = -1;
    else if (kind == USER_TASK_SHELL) user_shell_state.status = -1;
}

static int init_user_memory_layout() {
    if (user_memory_ready) return 0;
    if (ensure_user_region(&netdemo_state_region, USER_NETDEMO_STATE_PAGES) != 0) return -1;
    if (ensure_user_region(&netdemo_stack_region, USER_NETDEMO_STACK_PAGES) != 0) return -1;
    if (ensure_user_region(&netdemo_trap_stack_region, USER_KERNEL_TRAP_STACK_PAGES) != 0) return -1;
    if (ensure_user_region(&fetch_state_region, USER_FETCH_STATE_PAGES) != 0) return -1;
    if (ensure_user_region(&fetch_stack_region, USER_FETCH_STACK_PAGES) != 0) return -1;
    if (ensure_user_region(&fetch_trap_stack_region, USER_KERNEL_TRAP_STACK_PAGES) != 0) return -1;
    if (ensure_user_region(&shell_state_region, USER_SHELL_STATE_PAGES) != 0) return -1;
    if (ensure_user_region(&shell_stack_region, USER_SHELL_STACK_PAGES) != 0) return -1;
    if (ensure_user_region(&shell_trap_stack_region, USER_KERNEL_TRAP_STACK_PAGES) != 0) return -1;

    user_netdemo_state_ptr = (user_netdemo_state_t*)netdemo_state_region;
    user_fetch_state_ptr = (user_fetch_state_t*)fetch_state_region;
    user_shell_state_ptr = (user_shell_state_t*)shell_state_region;

    if (paging_map_user_region(USER_NETDEMO_STATE_VA, (uintptr_t)netdemo_state_region,
                               USER_NETDEMO_STATE_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_NETDEMO_STACK_VA, (uintptr_t)netdemo_stack_region,
                               USER_NETDEMO_STACK_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_FETCH_STATE_VA, (uintptr_t)fetch_state_region,
                               USER_FETCH_STATE_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_FETCH_STACK_VA, (uintptr_t)fetch_stack_region,
                               USER_FETCH_STACK_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_SHELL_STATE_VA, (uintptr_t)shell_state_region,
                               USER_SHELL_STATE_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    if (paging_map_user_region(USER_SHELL_STACK_VA, (uintptr_t)shell_stack_region,
                               USER_SHELL_STACK_PAGES * USER_PAGE_SIZE, PAGING_FLAG_WRITE) != 0) return -1;
    user_memory_ready = 1;
    return 0;
}

static void init_user_context(arch_trap_frame_t* context, uintptr_t user_stack_base, uint32_t user_stack_pages,
                              uintptr_t entry_point, uintptr_t user_state_ptr) {
    arch_user_frame_init(context, entry_point,
                         user_stack_base + (uintptr_t)user_stack_pages * USER_PAGE_SIZE - 16U);
    arch_user_frame_set_task_arg(context, user_state_ptr);
}

static int build_process_initial_stack(process_t* proc, const exec_image_t* image) {
    uintptr_t arg_ptrs[PROCESS_MAX_ARGS];
    uintptr_t sp;
    uintptr_t argv_base;

    if (!proc || !image) return -1;
    if (proc->user_argc < 0 || proc->user_argc > PROCESS_MAX_ARGS) return -1;

    sp = image->stack_top;
    for (int i = proc->user_argc - 1; i >= 0; i--) {
        uint32_t len = (uint32_t)strlen(proc->user_args[i]) + 1U;
        if (len > PROCESS_MAX_ARG_LEN) return -1;
        if (sp < (uintptr_t)image->stack_base + len) return -1;
        sp -= len;
        memcpy((void*)sp, proc->user_args[i], len);
        arg_ptrs[i] = sp;
    }

    if (image->image_class == EXEC_IMAGE_CLASS_ELF64) {
        sp &= ~(uintptr_t)0xFUL;
        if (sp < (uintptr_t)image->stack_base + sizeof(uint64_t)) return -1;
        sp -= sizeof(uint64_t);
        *(uint64_t*)sp = 0ULL;
        for (int i = proc->user_argc - 1; i >= 0; i--) {
            if (sp < (uintptr_t)image->stack_base + sizeof(uint64_t)) return -1;
            sp -= sizeof(uint64_t);
            *(uint64_t*)sp = (uint64_t)arg_ptrs[i];
        }
        argv_base = sp;
        if (sp < (uintptr_t)image->stack_base + sizeof(uint64_t)) return -1;
        sp -= sizeof(uint64_t);
        *(uint64_t*)sp = (uint64_t)proc->user_argc;
        arch_user_frame_set_exec_start(&proc->arch.user_frame, (uintptr_t)proc->user_argc, argv_base, sp);
        return 0;
    }

    sp &= ~(uintptr_t)0x3U;
    if (sp < (uintptr_t)image->stack_base + 4U) return -1;
    sp -= 4U;
    *(uint32_t*)sp = 0U;
    for (int i = proc->user_argc - 1; i >= 0; i--) {
        if (sp < (uintptr_t)image->stack_base + 4U) return -1;
        sp -= 4U;
        *(uint32_t*)sp = (uint32_t)arg_ptrs[i];
    }
    argv_base = sp;
    if (sp < (uintptr_t)image->stack_base + 4U) return -1;
    sp -= 4U;
    *(uint32_t*)sp = (uint32_t)proc->user_argc;
    arch_user_frame_set_exec_start(&proc->arch.user_frame, (uintptr_t)proc->user_argc, argv_base, sp);
    return 0;
}

int usermode_prepare_process_context(process_t* proc) {
    exec_image_t image;

    if (!proc) return -1;
    if (exec_activate_address_space(&proc->user_space) != EXEC_OK) return -1;
    if (exec_query_image(&proc->user_space, &image) != EXEC_OK) return -1;

    arch_user_frame_init(&proc->arch.user_frame, image.entry_point, image.stack_top);
    arch_user_frame_set_exec_class(&proc->arch.user_frame, image.image_class);
    proc->user_entry = image.entry_point;
    proc->user_stack_top = image.stack_top;
    return build_process_initial_stack(proc, &image);
}

static void sanitize_user_context(arch_trap_frame_t* context) {
    arch_user_frame_sanitize(context);
}

static int parse_http_target(const char* target, char* host, int host_len, char* path, int path_len) {
    const char* cursor = target;
    int host_off = 0;
    int path_off = 0;

    if (!target || !host || !path || host_len <= 1 || path_len <= 1) return -1;
    while (*cursor == ' ') cursor++;
    if (strncmp(cursor, "http://", 7) == 0) cursor += 7;
    else if (strncmp(cursor, "https://", 8) == 0) cursor += 8;

    while (*cursor && *cursor != ' ' && *cursor != '/') {
        if (host_off + 1 >= host_len) return -1;
        host[host_off++] = *cursor++;
    }
    host[host_off] = '\0';
    if (host_off == 0) return -1;

    if (*cursor == '/') {
        while (*cursor && *cursor != ' ') {
            if (path_off + 1 >= path_len) return -1;
            path[path_off++] = *cursor++;
        }
    } else {
        while (*cursor == ' ') cursor++;
        if (*cursor != '\0') {
            if (*cursor != '/') {
                if (path_off + 1 >= path_len) return -1;
                path[path_off++] = '/';
            }
            while (*cursor) {
                if (path_off + 1 >= path_len) return -1;
                path[path_off++] = *cursor++;
            }
        }
    }

    if (path_off == 0) path[path_off++] = '/';
    path[path_off] = '\0';
    return 0;
}

static int parse_host_only(const char* text, char* host, int host_len) {
    const char* cursor = text;
    int off = 0;

    if (!text || !host || host_len <= 1) return -1;
    while (*cursor == ' ') cursor++;
    if (strncmp(cursor, "http://", 7) == 0) cursor += 7;
    else if (strncmp(cursor, "https://", 8) == 0) cursor += 8;
    while (*cursor && *cursor != ' ' && *cursor != '/') {
        if (off + 1 >= host_len) return -1;
        host[off++] = *cursor++;
    }
    host[off] = '\0';
    return off != 0 ? 0 : -1;
}

static int next_arg_token(const char* src, int* io_off, char* out, int out_len) {
    int off = *io_off;
    int len = 0;

    if (!src || !io_off || !out || out_len <= 1) return -1;
    while (src[off] == ' ') off++;
    if (src[off] == '\0') return -1;

    while (src[off] != '\0' && src[off] != ' ') {
        if (len + 1 >= out_len) return -1;
        out[len++] = src[off++];
    }
    out[len] = '\0';
    *io_off = off;
    return 0;
}

static int parse_fetch_args(const char* args, char* host, int host_len,
                            char* path, int path_len, char* output_path, int output_path_len,
                            uint32_t* out_use_https) {
    char token0[128];
    char token1[128];
    char token2[128];
    int off = 0;
    int token_count = 0;

    if (out_use_https) *out_use_https = 0U;
    token0[0] = '\0';
    token1[0] = '\0';
    token2[0] = '\0';
    if (next_arg_token(args, &off, token0, sizeof(token0)) != 0) return -1;
    token_count++;
    if (next_arg_token(args, &off, token1, sizeof(token1)) != 0) return -1;
    token_count++;
    if (next_arg_token(args, &off, token2, sizeof(token2)) == 0) token_count++;
    while (args[off] == ' ') off++;
    if (args[off] != '\0') return -1;

    if (token_count == 2) {
        if (strncmp(token0, "https://", 8) == 0 && out_use_https) *out_use_https = 1U;
        if (parse_http_target(token0, host, host_len, path, path_len) != 0) return -1;
        strncpy(output_path, token1, (size_t)(output_path_len - 1));
        output_path[output_path_len - 1] = '\0';
        return 0;
    }
    if (token_count == 3) {
        if (strncmp(token0, "https://", 8) == 0 && out_use_https) *out_use_https = 1U;
        if (parse_host_only(token0, host, host_len) != 0) return -1;
        strncpy(path, token1, (size_t)(path_len - 1));
        path[path_len - 1] = '\0';
        strncpy(output_path, token2, (size_t)(output_path_len - 1));
        output_path[output_path_len - 1] = '\0';
        if (path[0] != '/') return -1;
        return 0;
    }
    return -1;
}

static void dispatch_user_task(user_task_kind_t kind, arch_trap_frame_t* context) {
    process_t* current = process_current();
    user_runtime_state_t saved_state;
    uintptr_t resume_stack_top = current ? current->arch.kernel_stack_top : (uintptr_t)KERNEL_BOOT_STACK_TOP;
    uintptr_t trap_stack_top = user_task_trap_stack_top(kind);
    uintptr_t stack_base = 0;
    uint32_t stack_pages = 0;

    if (kind == USER_TASK_NETDEMO) {
        stack_base = USER_NETDEMO_STACK_VA;
        stack_pages = USER_NETDEMO_STACK_PAGES;
    } else if (kind == USER_TASK_FETCH) {
        stack_base = USER_FETCH_STACK_VA;
        stack_pages = USER_FETCH_STACK_PAGES;
    } else if (kind == USER_TASK_SHELL) {
        stack_base = USER_SHELL_STACK_VA;
        stack_pages = USER_SHELL_STACK_PAGES;
    }

    if (activate_builtin_user_space() != 0) {
        invalidate_user_task(kind);
        return;
    }

    if (!validate_user_context(context, stack_base, stack_pages)) {
        serial_write("[user] invalid context kind=");
        serial_write_hex32((uint32_t)kind);
        serial_write(" eip=");
        serial_write_hex_uintptr(context ? arch_frame_user_ip(context) : 0U);
        serial_write(" esp=");
        serial_write_hex_uintptr(context ? arch_frame_user_sp(context) : 0U);
        serial_write_char('\n');
        vga_print_color("user task invalid context: ", 0x0C);
        vga_println(user_task_kind_name(kind));
        invalidate_user_task(kind);
        return;
    }

    save_user_runtime_state(&saved_state);
    active_user_task = kind;
    /* User traps must land on a clean kernel stack; otherwise INT frames overwrite
       the suspended process stack frame that run_user_task will later resume. */
    arch_set_kernel_stack(trap_stack_top);
#if USERMODE_TRACE_DISPATCH
    serial_write("[user] dispatch kind=");
    serial_write_hex32((uint32_t)kind);
    serial_write(" eip=");
    serial_write_hex_uintptr(context ? arch_frame_user_ip(context) : 0U);
    serial_write(" esp=");
    serial_write_hex_uintptr(context ? arch_frame_user_sp(context) : 0U);
    serial_write_char('\n');
#endif
    sanitize_user_context(context);
    user_current_task_frame_ptr = context;
    arch_enter_user(context);
    restore_user_runtime_state(&saved_state);
    arch_set_kernel_stack(resume_stack_top);
}

int usermode_run_external_process(process_t* proc) {
    process_t* current = process_current();
    user_runtime_state_t saved_state;
    uintptr_t resume_stack_top = current ? current->arch.kernel_stack_top : (uintptr_t)KERNEL_BOOT_STACK_TOP;
    uintptr_t trap_stack_top;

    if (!proc) return -1;
    if (exec_activate_address_space(&proc->user_space) != EXEC_OK) return -1;
    if (!validate_process_context(proc)) {
        serial_write("[user] invalid process context pid=");
        serial_write_hex32((uint32_t)proc->pid);
        serial_write(" eip=");
        serial_write_hex_uintptr(arch_frame_user_ip(&proc->arch.user_frame));
        serial_write(" esp=");
        serial_write_hex_uintptr(arch_frame_user_sp(&proc->arch.user_frame));
        serial_write_char('\n');
        return -1;
    }

    save_user_runtime_state(&saved_state);
    active_user_task = USER_TASK_PROCESS;
    active_external_process = proc;
    user_kernel_return_mode = USER_KERNEL_RETURN_NONE;
    trap_stack_top = user_task_trap_stack_top(USER_TASK_PROCESS);
    arch_set_kernel_stack(trap_stack_top);
    sanitize_user_context(&proc->arch.user_frame);
    user_current_task_frame_ptr = &proc->arch.user_frame;
    arch_enter_user(&proc->arch.user_frame);
    restore_user_runtime_state(&saved_state);
    arch_set_kernel_stack(resume_stack_top);
    return 0;
}

static int run_sync_user_app(user_task_kind_t kind, arch_trap_frame_t* context, int* status_ptr) {
    uint32_t last_clock_tick = timer_ticks;
    int last_mx = get_mouse_x();
    int last_my = get_mouse_y();
    int last_lp = mouse_left_pressed();
    int last_rp = mouse_right_pressed();

    while (*status_ptr == USER_APP_STATUS_RUNNING) {
        process_t* current;
        int mx;
        int my;
        int lp;
        int rp;
        int needs_present = 0;

        dispatch_user_task(kind, context);

        current = process_current();
        (void)current;
        if (!screen_is_graphics_enabled()) {
            continue;
        }

        if (timer_ticks - last_clock_tick >= 100U) {
            read_rtc();
            last_clock_tick = timer_ticks;
            gui_needs_redraw = 1;
        }

        mx = get_mouse_x();
        my = get_mouse_y();
        lp = mouse_left_pressed();
        rp = mouse_right_pressed();

        if (mx != last_mx || my != last_my || lp != last_lp || rp != last_rp) {
            needs_present = 1;
        }
        if (gui_needs_redraw) needs_present = 1;

        if (needs_present) {
            vbe_compose_scene_basic();
            gui_needs_redraw = 0;
            last_mx = mx;
            last_my = my;
            last_lp = lp;
            last_rp = rp;
        }
    }
    return *status_ptr;
}

int init_usermode() {
    user_memory_ready = 0;
    if (init_user_memory_layout() != 0) return -1;
    memset(&netdemo_context, 0, sizeof(netdemo_context));
    memset(&fetch_context, 0, sizeof(fetch_context));
    memset(&shell_context, 0, sizeof(shell_context));
    memset(&user_netdemo_state, 0, sizeof(user_netdemo_state));
    memset(&user_fetch_state, 0, sizeof(user_fetch_state));
    memset(&user_shell_state, 0, sizeof(user_shell_state));
    active_user_task = USER_TASK_NONE;
    serial_write_line("[user] init builtin user helpers");
    return 0;
}

void run_user_tasks() {
    /* Legacy GUI apps now run as normal /bin ELF processes. */
}

void stop_all_background_user_tasks() {
}

int run_user_netdemo(const char* target) {
    const char* resolved_target = (target && target[0] != '\0') ? target : "example.com /";
    int status;

    memset(&user_netdemo_state, 0, sizeof(user_netdemo_state));
    if (parse_http_target(resolved_target,
                          user_netdemo_state.host, sizeof(user_netdemo_state.host),
                          user_netdemo_state.path, sizeof(user_netdemo_state.path)) != 0) {
        vga_print_color("Usage: netdemo <host> [path]\n", 0x0E);
        return -1;
    }

    user_netdemo_state.status = USER_APP_STATUS_RUNNING;
    user_netdemo_state.use_https = 0U;
    init_user_context(&netdemo_context, USER_NETDEMO_STACK_VA, USER_NETDEMO_STACK_PAGES,
                      (uintptr_t)user_netdemo_entry_gate, USER_NETDEMO_STATE_VA);
    status = run_sync_user_app(USER_TASK_NETDEMO, &netdemo_context, &user_netdemo_state.status);
    if (status < 0) {
        vga_print_color("netdemo: ", 0x0C);
        vga_println(net_strerror(status));
    }
    return status;
}

int run_user_https_command(const char* target) {
    int status;

    memset(&user_netdemo_state, 0, sizeof(user_netdemo_state));
    if (parse_http_target(target,
                          user_netdemo_state.host, sizeof(user_netdemo_state.host),
                          user_netdemo_state.path, sizeof(user_netdemo_state.path)) != 0) {
        vga_print_color("Usage: https https://<pinned-host>/<path>\n", 0x0E);
        return -1;
    }

    user_netdemo_state.status = USER_APP_STATUS_RUNNING;
    user_netdemo_state.use_https = 1U;
    init_user_context(&netdemo_context, USER_NETDEMO_STACK_VA, USER_NETDEMO_STACK_PAGES,
                      (uintptr_t)user_netdemo_entry_gate, USER_NETDEMO_STATE_VA);
    status = run_sync_user_app(USER_TASK_NETDEMO, &netdemo_context, &user_netdemo_state.status);
    if (status < 0) {
        vga_print_color("https: ", 0x0C);
        vga_println(net_strerror(status));
        vga_print("https stage   : ");
        vga_println(user_https_stage_name(user_netdemo_state.debug_stage));
        return status;
    }

    vga_print("HTTPS GET       : ");
    vga_println(target);
    vga_print("Resolved        : ");
    net_print_ip(user_netdemo_state.result.resolved_ip);
    vga_println("");
    vga_println("---- response ----");
    if (user_netdemo_state.result.response_len != 0U) vga_println(user_netdemo_state.response);
    else vga_println("(empty response)");
    if (user_netdemo_state.result.truncated != 0U) {
        vga_print_color("warning: Response truncated to local buffer size.\n", 0x0E);
    }
    if (user_netdemo_state.result.complete == 0U) {
        vga_print_color("warning: Remote peer did not close cleanly before timeout.\n", 0x0E);
    }
    return status;
}

int run_user_fetch(const char* args) {
    int status;

    memset(&user_fetch_state, 0, sizeof(user_fetch_state));
    if (parse_fetch_args(args,
                         user_fetch_state.host, sizeof(user_fetch_state.host),
                         user_fetch_state.path, sizeof(user_fetch_state.path),
                         user_fetch_state.output_path, sizeof(user_fetch_state.output_path),
                         &user_fetch_state.use_https) != 0) {
        vga_print_color("Usage: fetch <host> [path] <output-file>\n", 0x0E);
        vga_print_color("   or: fetch https://<pinned-host>/<path> <output-file>\n", 0x0E);
        return -1;
    }

    user_fetch_state.status = USER_APP_STATUS_RUNNING;
    init_user_context(&fetch_context, USER_FETCH_STACK_VA, USER_FETCH_STACK_PAGES,
                      (uintptr_t)user_fetch_entry_gate, USER_FETCH_STATE_VA);
    status = run_sync_user_app(USER_TASK_FETCH, &fetch_context, &user_fetch_state.status);
    if (status < 0) {
        vga_print_color("fetch: ", 0x0C);
        vga_println(net_strerror(status));
        return status;
    }

    vga_print("Saved ");
    vga_print_int((int)user_fetch_state.saved_len);
    vga_print(" bytes to ");
    vga_println(user_fetch_state.output_path);
    if (user_fetch_state.result.truncated != 0U) {
        vga_print_color("warning: Response truncated to local fetch buffer.\n", 0x0E);
    }
    if (user_fetch_state.result.complete == 0U) {
        vga_print_color("warning: Remote peer did not close cleanly before timeout.\n", 0x0E);
    }
    return status;
}

int run_user_shell_command(const char* command) {
    int status;

    memset(&user_shell_state, 0, sizeof(user_shell_state));
    if (command) {
        strncpy(user_shell_state.command, command, sizeof(user_shell_state.command) - 1U);
        user_shell_state.command[sizeof(user_shell_state.command) - 1U] = '\0';
    }

    user_shell_state.status = USER_APP_STATUS_RUNNING;
    user_shell_state.exit_code = 0;
    init_user_context(&shell_context, USER_SHELL_STACK_VA, USER_SHELL_STACK_PAGES,
                      (uintptr_t)user_shell_entry_gate, USER_SHELL_STATE_VA);
    status = run_sync_user_app(USER_TASK_SHELL, &shell_context, &user_shell_state.status);
    return status == USER_APP_STATUS_OK ? user_shell_state.exit_code : status;
}

void usermode_debug_dump(const char* tag) {
    serial_write("[user] dump ");
    serial_write(tag ? tag : "");
    serial_write(" active=");
    serial_write_hex32((uint32_t)active_user_task);
    serial_write_char('\n');
}

int usermode_exit_current_task(int exit_code) {
    switch (active_user_task) {
        case USER_TASK_NETDEMO:
            user_netdemo_state.status = exit_code == 0 ? USER_APP_STATUS_OK : exit_code;
            return 0;
        case USER_TASK_FETCH:
            user_fetch_state.status = exit_code == 0 ? USER_APP_STATUS_OK : exit_code;
            return 0;
        case USER_TASK_SHELL:
            user_shell_state.exit_code = exit_code;
            user_shell_state.status = USER_APP_STATUS_OK;
            return 0;
        default:
            return -1;
    }
}

int usermode_schedule_current_process_exit(int exit_code) {
    if (active_user_task != USER_TASK_PROCESS || !active_external_process) return -1;
    active_external_process->exit_code = exit_code;
    active_external_process->flags |= PROCESS_FLAG_USER_EXIT_PENDING;
    user_kernel_return_mode = USER_KERNEL_RETURN_KERNEL;
    return 0;
}

void user_yield_handler(arch_trap_frame_t* frame) {
    if (active_user_task == USER_TASK_NETDEMO) {
        netdemo_context = *frame;
    } else if (active_user_task == USER_TASK_FETCH) {
        fetch_context = *frame;
    } else if (active_user_task == USER_TASK_SHELL) {
        shell_context = *frame;
    } else if (active_user_task == USER_TASK_PROCESS) {
        if (active_external_process) active_external_process->arch.user_frame = *frame;
    }
}
