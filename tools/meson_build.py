#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


USER_PROGRAMS = [
    "hello", "ps", "cat", "echo", "kill", "proc_test", "pipe_test",
    "credits", "neofetch", "desktop", "explorer", "narcpad", "settings",
    "taskmgr", "snake", "doom", "core_tools", "tls_tools", "posix_smoke", "args_smoke",
    "clear", "date", "dns", "help", "http", "ls", "net", "netdemo",
    "ping", "pwd", "time", "uptime", "ver",
]
USER_EMBED_PROGRAMS = [
    "neofetch", "credits", "desktop", "explorer", "narcpad", "settings",
    "taskmgr", "snake", "core_tools",
]
FS_SEED_PROGRAMS = [p for p in USER_PROGRAMS if p not in ("doom", "posix_smoke")]
USER_TLS_PROGRAMS = ["tls_tools"]
USER_TLS_SOURCES = [
    "kernel/apps/user_tls.c",
    "kernel/apps/user_tls_crypto.c",
    "kernel/apps/user_tls_bigint.c",
    "kernel/apps/user_tls_x509.c",
    "kernel/apps/user_tls_pins.c",
]
USER_TLS_HEADERS = [
    "kernel/apps/user_tls.h",
    "kernel/apps/user_tls_crypto.h",
    "kernel/apps/user_tls_bigint.h",
    "kernel/apps/user_tls_x509.h",
    "kernel/apps/user_tls_pins.h",
    "kernel/apps/user_string.h",
]
DOOMGENERIC_SOURCE_NAMES = [
    "dummy.c", "am_map.c", "doomdef.c", "doomstat.c", "dstrings.c",
    "d_event.c", "d_items.c", "d_iwad.c", "d_loop.c", "d_main.c",
    "d_mode.c", "d_net.c", "f_finale.c", "f_wipe.c", "g_game.c",
    "hu_lib.c", "hu_stuff.c", "info.c", "i_cdmus.c", "i_endoom.c",
    "i_joystick.c", "i_scale.c", "i_sound.c", "i_system.c", "i_timer.c",
    "memio.c", "m_argv.c", "m_bbox.c", "m_cheat.c", "m_config.c",
    "m_controls.c", "m_fixed.c", "m_menu.c", "m_misc.c", "m_random.c",
    "p_ceilng.c", "p_doors.c", "p_enemy.c", "p_floor.c", "p_inter.c",
    "p_lights.c", "p_map.c", "p_maputl.c", "p_mobj.c", "p_plats.c",
    "p_pspr.c", "p_saveg.c", "p_setup.c", "p_sight.c", "p_spec.c",
    "p_switch.c", "p_telept.c", "p_tick.c", "p_user.c", "r_bsp.c",
    "r_data.c", "r_draw.c", "r_main.c", "r_plane.c", "r_segs.c",
    "r_sky.c", "r_things.c", "sha1.c", "sounds.c", "statdump.c",
    "st_lib.c", "st_stuff.c", "s_sound.c", "tables.c", "v_video.c",
    "wi_stuff.c", "w_checksum.c", "w_file.c", "w_main.c", "w_wad.c",
    "z_zone.c", "w_file_stdc.c", "i_input.c", "i_video.c",
    "doomgeneric.c",
]
ICON_SPECS = [
    ("folder", "folder_icon"),
    ("text", "text_icon"),
    ("settings", "settings_icon"),
    ("this_pc", "this_pc_icon"),
    ("snake", "snake_icon"),
    ("doom", "doom_icon"),
    ("terminal", "terminal_icon"),
]

BOOT_MANIFEST_LBA = 17
KERNEL_START_LBA = 18
DOOM_BIN_LBA = 8192
DOOM_BIN_MAX_SIZE = 1048576
DOOM1_WAD_MAX_SIZE = 4489216
INITRD_ADDR = "0x00A00000"


def rel(path):
    return Path(path).as_posix()


def run(cmd, cwd=None):
    print("+ " + " ".join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], cwd=cwd, check=True)


def capture(cmd):
    return subprocess.check_output([str(c) for c in cmd], text=True).strip()


def newer(output, inputs):
    output = Path(output)
    if not output.exists():
        return True
    out_mtime = output.stat().st_mtime
    for item in inputs:
        path = Path(item)
        if path.exists() and path.stat().st_mtime > out_mtime:
            return True
    return False


def size(path):
    path = Path(path)
    return path.stat().st_size if path.exists() else 0


def sectors(path_or_size):
    value = size(path_or_size) if not isinstance(path_or_size, int) else path_or_size
    return (value + 511) // 512


class Build:
    def __init__(self, args):
        self.src = Path(args.source_root).resolve()
        self.build = Path(args.build_root).resolve()
        self.cc = args.cc
        self.ld = args.ld
        self.as_ = args.nasm
        self.objcopy = args.objcopy
        self.vbe_width = args.vbe_width
        self.vbe_height = args.vbe_height
        self.disk_image_sectors = args.disk_image_sectors
        self.autorun_posix_smoke = args.autorun_posix_smoke
        self.musl_dir = (self.src / args.musl_dir).resolve()
        self.musl_prefix = args.musl_prefix
        self.kernel_dirs = [self.src / "kernel"] + sorted(p for p in (self.src / "kernel").rglob("*") if p.is_dir())
        self.kernel_headers = sorted((self.src / "kernel").rglob("*.h"))
        self.user_include_headers = sorted((self.src / "user/include").rglob("*.h"))
        self.user_headers = sorted((self.src / "user/programs").glob("*.h"))
        self.doom_port_dir = self.src / "user/ports/doom"
        self.doomgeneric_dir = self.doom_port_dir / "doomgeneric"
        self.doom_headers = sorted(self.doom_port_dir.rglob("*.h"))
        self.doom1_wad = self.doom_port_dir / "doom1.wad"

    def obj_dir(self, arch):
        return self.build / "obj" / arch

    def arch_cfg(self, arch):
        if arch == "i386":
            return {
                "gcc_m": "-m32",
                "nasm_elf": "elf32",
                "ld_m": "elf_i386",
                "linker": self.src / "linker_i386.ld",
                "user_linker": self.src / "user/linker.ld",
                "kernel_elf": self.obj_dir(arch) / "kernel.elf",
                "kernel_bin": self.obj_dir(arch) / "kernel.bin",
                "image": self.obj_dir(arch) / "minios.img",
                "iso": self.obj_dir(arch) / "narcos-i386.iso",
                "usb": self.obj_dir(arch) / "narcos-i386-usb.img",
                "stage2_src": self.src / "boot/stage2.asm",
                "crt_src": self.src / "user/crt0.asm",
                "crt_obj": self.obj_dir(arch) / "user/crt0.o",
                "musl_crt_src": self.musl_dir / "crt/narcos-crt1-i386.s",
                "musl_crt": self.obj_dir(arch) / "user/musl-crt1.o",
                "kernel_ldflags": ["-m", "elf_i386", "-T", self.src / "linker_i386.ld", "-nostdlib", "-s", "--strip-all"],
                "user_ldflags": ["-m", "elf_i386", "-T", self.src / "user/linker.ld", "-nostdlib", "-s", "--strip-all"],
                "kernel_bg": ("96x54^", "96x54"),
                "desktop_bg": ("320x180^", "320x180"),
                "iso_volume": "NARCOS_I386",
                "iso_readme": "NarcOs i386 bootable ISO\nBoot image: /boot/minios.img\n",
                "iso_boot_img": "minios.img",
                "qemu": "qemu-system-i386",
                "run_display": [],
            }
        return {
            "gcc_m": "-m64",
            "nasm_elf": "elf64",
            "ld_m": "elf_x86_64",
            "linker": self.src / "linker_x86_64.ld",
            "user_linker": self.src / "user/linker_x86_64.ld",
            "kernel_elf": self.obj_dir(arch) / "kernel64.elf",
            "kernel_bin": self.obj_dir(arch) / "kernel64.bin",
            "image": self.obj_dir(arch) / "minios64.img",
            "iso": self.obj_dir(arch) / "narcos-x86_64.iso",
            "usb": self.obj_dir(arch) / "narcos-x86_64-usb.img",
            "stage2_src": self.src / "boot/stage2_x86_64.asm",
            "crt_src": self.src / "user/crt0_x86_64.asm",
            "crt_obj": self.obj_dir(arch) / "user/crt0_x86_64.o",
            "musl_crt_src": self.musl_dir / "crt/narcos-crt1-x86_64.s",
            "musl_crt": self.obj_dir(arch) / "user/musl-crt1.o",
            "kernel_ldflags": ["-m", "elf_x86_64", "-T", self.src / "linker_x86_64.ld", "-nostdlib"],
            "user_ldflags": ["-m", "elf_x86_64", "-T", self.src / "user/linker_x86_64.ld", "-nostdlib", "-s", "--strip-all"],
            "kernel_bg": ("160x90^", "160x90"),
            "desktop_bg": ("224x126^", "224x126"),
            "iso_volume": "NARCOS_X86_64",
            "iso_readme": "NarcOs x86_64 bootable ISO\nBoot image: /boot/minios64.img\n",
            "iso_boot_img": "minios64.img",
            "qemu": "qemu-system-x86_64",
            "run_display": ["-display", "none"],
        }

    def common_cflags(self):
        flags = [
            "-ffreestanding", "-fno-pie", "-fno-pic", "-fno-stack-protector",
            "-fcf-protection=none", "-fno-builtin", "-fno-strict-aliasing",
            "-Wall", "-Wextra",
        ]
        if self.autorun_posix_smoke:
            flags.append("-DNARCOS_AUTORUN_POSIX_SMOKE=1")
        return flags

    def kernel_include_flags(self):
        return ["-I" + str(path) for path in self.kernel_dirs]

    def gcc_include_dir(self, arch):
        return capture([self.cc, self.arch_cfg(arch)["gcc_m"], "-print-file-name=include"])

    def libgcc(self, arch):
        return capture([self.cc, self.arch_cfg(arch)["gcc_m"], "-print-libgcc-file-name"])

    def musl_paths(self, arch):
        root = self.obj_dir(arch)
        dest = root / "musl-root"
        return {
            "build": root / "musl-build",
            "dest": dest,
            "include": dest / self.musl_prefix.lstrip("/") / "include",
            "lib": dest / self.musl_prefix.lstrip("/") / "lib",
            "libc": dest / self.musl_prefix.lstrip("/") / "lib/libc.a",
        }

    def kernel_cflags(self, arch, extra=None):
        cfg = self.arch_cfg(arch)
        flags = [cfg["gcc_m"]] + self.common_cflags()
        if arch == "x86_64":
            flags += ["-I" + str(self.src / "kernel/arch/x86_64")]
        flags += self.kernel_include_flags()
        if arch == "i386":
            flags += ["-mpreferred-stack-boundary=2", "-mno-red-zone", "-Os", "-fomit-frame-pointer"]
        else:
            flags += [
                "-mno-red-zone", "-mgeneral-regs-only", "-mno-mmx", "-mno-sse",
                "-mno-sse2", "-msoft-float", "-O2", "-fomit-frame-pointer",
                "-falign-functions=1", "-falign-labels=1",
                "-falign-loops=1", "-falign-jumps=1",
            ]
        if extra:
            flags += extra
        return flags

    def user_cflags(self, arch, doom=False):
        cfg = self.arch_cfg(arch)
        musl = self.musl_paths(arch)
        flags = [cfg["gcc_m"]] + self.common_cflags() + ["-I" + str(self.src / "user/include")]
        if arch == "x86_64":
            flags += ["-I" + str(self.src / "kernel/arch/x86_64")]
        flags += self.kernel_include_flags()
        flags += [
            "-nostdinc", "-isystem", str(musl["include"]), "-isystem",
            self.gcc_include_dir(arch),
        ]
        if arch == "i386":
            flags += ["-mpreferred-stack-boundary=2", "-mno-red-zone", "-O2", "-fomit-frame-pointer"]
        else:
            flags += [
                "-mno-red-zone", "-mgeneral-regs-only", "-mno-mmx", "-mno-sse",
                "-mno-sse2", "-msoft-float", "-O2", "-fomit-frame-pointer",
                "-Wa,-mtune=i386",
                "-falign-functions=1", "-falign-labels=1",
                "-falign-loops=1", "-falign-jumps=1",
            ]
        if doom:
            doom_flags = [
                "-I" + str(self.doom_port_dir / "include"),
                "-I" + str(self.doomgeneric_dir),
                "-include", "string.h", "-include", "strings.h", "-DNORMALUNIX",
                "-D_DEFAULT_SOURCE", "-DDOOMGENERIC_RESX=320",
                "-DDOOMGENERIC_RESY=200", "-Wno-unused-parameter",
                "-Wno-missing-field-initializers", "-Wno-sign-compare",
            ]
            if arch == "x86_64":
                doom_flags.append("-DNARCOS_DOOM_NO_FLOAT")
            flags = doom_flags + flags
        return flags

    def source_list(self, arch, kind):
        sources = sorted((self.src / "kernel").rglob("*." + kind))
        rel_sources = [rel(p.relative_to(self.src)) for p in sources]
        if arch == "i386":
            excludes = set(USER_TLS_SOURCES)
            excludes.update([
                "kernel/apps/user_explorer.c",
                "kernel/apps/user_narcpad.c",
                "kernel/apps/user_settings.c",
                "kernel/apps/user_snake_app.c",
                "kernel/apps/user_explorer_entry.asm",
                "kernel/apps/user_narcpad_entry.asm",
                "kernel/apps/user_settings_entry.asm",
                "kernel/apps/user_snake.asm",
            ])
            return [self.src / p for p in rel_sources if not p.startswith("kernel/arch/x86_64/") and p not in excludes]
        if kind == "c":
            excludes = set(USER_TLS_SOURCES)
            excludes.update([
                "kernel/arch/x86_64/display.c",
                "kernel/arch/x86_64/main.c",
                "kernel/arch/x86_64/stub.c",
                "kernel/arch/x86_64/stubs.c",
                "kernel/arch/x86_64/user_snake.c",
                "kernel/arch/x86_64/usermode.c",
                "kernel/apps/user_explorer.c",
                "kernel/apps/user_narcpad.c",
                "kernel/apps/user_settings.c",
                "kernel/apps/user_snake_app.c",
                "kernel/drivers/platform/serial.c",
                "kernel/mm/memory_alloc.c",
            ])
        else:
            excludes = {
                "kernel/apps/user_explorer_entry.asm",
                "kernel/apps/user_fetch.asm",
                "kernel/apps/user_narcpad_entry.asm",
                "kernel/apps/user_netdemo.asm",
                "kernel/apps/user_settings_entry.asm",
                "kernel/apps/user_shell_entry.asm",
                "kernel/apps/user_snake.asm",
                "kernel/apps/user_test.asm",
            }
        return [self.src / p for p in rel_sources if not p.startswith("kernel/arch/x86/") and p not in excludes]

    def obj_for_source(self, arch, source, prefix=None):
        source = Path(source)
        if prefix:
            stem = source.with_suffix("").name
            return self.obj_dir(arch) / prefix / (stem + ".o")
        rel_path = source.relative_to(self.src / "kernel").with_suffix(".o")
        return self.obj_dir(arch) / rel_path

    def compile_c(self, arch, source, output, flags, deps=None):
        deps = [source] + list(deps or [])
        output.parent.mkdir(parents=True, exist_ok=True)
        if newer(output, deps):
            run([self.cc] + flags + ["-c", source, "-o", output])

    def assemble(self, arch, source, output, fmt=None, defines=None):
        output.parent.mkdir(parents=True, exist_ok=True)
        if newer(output, [source]):
            cmd = [self.as_]
            for key, value in (defines or {}).items():
                cmd.append(f"-D{key}={value}")
            cmd += ["-f", fmt or self.arch_cfg(arch)["nasm_elf"], source, "-o", output]
            run(cmd)

    def link_binary_object(self, arch, input_path, output):
        output.parent.mkdir(parents=True, exist_ok=True)
        try:
            input_arg = Path(input_path).resolve().relative_to(self.build)
        except ValueError:
            input_arg = Path(input_path).resolve()
        run([self.ld, "-r", "-b", "binary", "-m", self.arch_cfg(arch)["ld_m"], input_arg, "-o", output], cwd=self.build)

    def apply_musl_overlay(self):
        run([sys.executable, self.src / "tools/apply_musl_narcos_overlay.py", self.musl_dir])

    def build_musl(self, arch):
        cfg = self.arch_cfg(arch)
        paths = self.musl_paths(arch)
        self.apply_musl_overlay()
        if not paths["libc"].exists():
            paths["build"].mkdir(parents=True, exist_ok=True)
            target = "i386-linux-musl" if arch == "i386" else "x86_64-linux-musl"
            cflags = [
                "-fno-pie", "-fno-pic", "-fno-stack-protector",
                "-fcf-protection=none", "-fno-builtin", "-fno-strict-aliasing",
                "-mno-red-zone", "-O2", "-fomit-frame-pointer",
            ]
            if arch == "i386":
                cflags.insert(-2, "-mpreferred-stack-boundary=2")
            run([
                self.musl_dir / "configure",
                "--srcdir=" + str(self.musl_dir),
                "--prefix=" + self.musl_prefix,
                "--target=" + target,
                "--disable-shared",
                "CC=" + self.cc + " " + cfg["gcc_m"],
                "AR=ar",
                "RANLIB=ranlib",
                "CFLAGS=" + " ".join(cflags),
            ], cwd=paths["build"])
            run(["make", "-C", paths["build"], "ARCH=" + arch])
            run([
                "make", "-C", paths["build"], "ARCH=" + arch,
                "DESTDIR=" + str(paths["dest"]), "install-headers", "install-libs",
            ])
            print(f"[OK] {arch} musl: {paths['libc']}")
        crt_inputs = [cfg["musl_crt_src"]]
        if newer(cfg["musl_crt"], crt_inputs):
            cfg["musl_crt"].parent.mkdir(parents=True, exist_ok=True)
            crt_flags = [cfg["gcc_m"], "-fno-pie", "-fno-pic", "-fno-stack-protector"]
            if arch == "x86_64":
                crt_flags.append("-mno-red-zone")
            run([self.cc] + crt_flags + ["-c", cfg["musl_crt_src"], "-o", cfg["musl_crt"]])

    def build_asset_objects(self, arch):
        cfg = self.arch_cfg(arch)
        objects = {}

        def magick_raw(src, raw, fmt, args):
            raw.parent.mkdir(parents=True, exist_ok=True)
            if newer(raw, [src]):
                run(["magick", src] + args + [f"{fmt}:{raw}"])

        bg_raw = self.obj_dir(arch) / "assets/bg.rgb"
        bg_obj = self.obj_dir(arch) / "assets/bg.o"
        magick_raw(self.src / "assets/bg.png", bg_raw, "rgb", [
            "-resize", cfg["kernel_bg"][0], "-gravity", "center", "-extent",
            cfg["kernel_bg"][1], "-alpha", "off", "-depth", "8",
        ])
        self.link_binary_object(arch, bg_raw, bg_obj)
        objects["kernel_bg"] = bg_obj

        logo_raw = self.obj_dir(arch) / "assets/logo.rgb"
        logo_obj = self.obj_dir(arch) / "assets/logo.o"
        magick_raw(self.src / "assets/logo.png", logo_raw, "rgb", [
            "-background", "#0B1016", "-alpha", "remove", "-alpha", "off",
            "-resize", "24x24^", "-gravity", "center", "-extent", "24x24",
            "-depth", "8",
        ])
        self.link_binary_object(arch, logo_raw, logo_obj)
        objects["kernel_logo"] = logo_obj

        desktop_raw = self.obj_dir(arch) / "user/assets/desktop_bg.rgb"
        desktop_obj = self.obj_dir(arch) / "user/assets/desktop_bg.o"
        magick_raw(self.src / "assets/bg.png", desktop_raw, "rgb", [
            "-filter", "Lanczos", "-resize", cfg["desktop_bg"][0], "-gravity",
            "center", "-extent", cfg["desktop_bg"][1], "-alpha", "off",
            "-depth", "8",
        ])
        self.link_binary_object(arch, desktop_raw, desktop_obj)
        objects["desktop_bg"] = desktop_obj

        icon_objects = {}
        for src_name, out_name in ICON_SPECS:
            raw = self.obj_dir(arch) / f"user/assets/{out_name}.rgba"
            obj = self.obj_dir(arch) / f"user/assets/{out_name}.o"
            magick_raw(self.src / f"assets/icon/{src_name}.png", raw, "rgba", [
                "-filter", "Lanczos", "-resize", "44x44", "-gravity", "center",
                "-background", "none", "-extent", "44x44", "-depth", "8",
            ])
            self.link_binary_object(arch, raw, obj)
            icon_objects[out_name] = obj
        objects["icons"] = icon_objects
        return objects

    def build_user_objects(self, arch, assets):
        self.build_musl(arch)
        cfg = self.arch_cfg(arch)
        musl = self.musl_paths(arch)
        self.assemble(arch, cfg["crt_src"], cfg["crt_obj"])

        program_objects = {}
        common_deps = (
            self.user_include_headers + self.user_headers + self.kernel_headers +
            [musl["libc"], self.src / "tools/meson_build.py"]
        )
        for program in USER_PROGRAMS:
            src = self.src / f"user/programs/{program}.c"
            obj = self.obj_dir(arch) / f"user/programs/{program}.o"
            deps = common_deps + (self.doom_headers if program == "doom" else [])
            self.compile_c(arch, src, obj, self.user_cflags(arch, doom=(program == "doom")), deps=deps)
            program_objects[program] = obj

        doom_libc = self.obj_dir(arch) / "user/programs/doom_libc.o"
        self.compile_c(arch, self.src / "user/programs/doom_libc.c", doom_libc, self.user_cflags(arch, doom=True), deps=common_deps + self.doom_headers)

        doom_objects = []
        for name in DOOMGENERIC_SOURCE_NAMES:
            src = self.doomgeneric_dir / name
            obj = self.obj_dir(arch) / "doomgeneric" / (Path(name).stem + ".o")
            self.compile_c(arch, src, obj, self.user_cflags(arch, doom=True), deps=common_deps + self.doom_headers)
            doom_objects.append(obj)

        tls_objects = []
        for source in USER_TLS_SOURCES:
            src = self.src / source
            obj = self.obj_dir(arch) / "user/lib" / (src.stem + ".o")
            self.compile_c(arch, src, obj, self.user_cflags(arch), deps=common_deps + [self.src / p for p in USER_TLS_HEADERS])
            tls_objects.append(obj)

        binaries = {}
        for program in USER_PROGRAMS:
            out = self.obj_dir(arch) / "user/bin" / program
            out.parent.mkdir(parents=True, exist_ok=True)
            if program == "doom":
                inputs = [cfg["musl_crt"], program_objects["doom"], doom_libc] + doom_objects + [musl["libc"], cfg["user_linker"]]
                objs = [cfg["musl_crt"], program_objects["doom"], doom_libc] + doom_objects
            elif program in USER_TLS_PROGRAMS:
                inputs = [cfg["musl_crt"], program_objects[program]] + tls_objects + [musl["libc"], cfg["user_linker"]]
                objs = [cfg["musl_crt"], program_objects[program]] + tls_objects
            elif program == "desktop":
                icon_objs = list(assets["icons"].values())
                inputs = [cfg["musl_crt"], program_objects[program], assets["desktop_bg"]] + icon_objs + [musl["libc"], cfg["user_linker"]]
                objs = [cfg["musl_crt"], program_objects[program], assets["desktop_bg"]] + icon_objs
            elif program == "explorer":
                inputs = [cfg["musl_crt"], program_objects[program], assets["icons"]["folder_icon"], assets["icons"]["text_icon"], musl["libc"], cfg["user_linker"]]
                objs = [cfg["musl_crt"], program_objects[program], assets["icons"]["folder_icon"], assets["icons"]["text_icon"]]
            else:
                inputs = [cfg["musl_crt"], program_objects[program], musl["libc"], cfg["user_linker"]]
                objs = [cfg["musl_crt"], program_objects[program]]
            if newer(out, inputs):
                run([
                    self.ld, *cfg["user_ldflags"], "-o", out, *objs,
                    "--start-group", musl["libc"], self.libgcc(arch), "--end-group",
                ])
                print(f"[OK] {arch} user: {out} ({size(out)} byte)")
            binaries[program] = out
        return binaries

    def payload_layout(self, arch):
        doom_bin = self.obj_dir(arch) / "user/bin/doom"
        posix = self.obj_dir(arch) / "user/bin/posix_smoke"
        tls = self.obj_dir(arch) / "user/bin/tls_tools"
        doom_size = size(doom_bin)
        doom_secs = sectors(doom_size)
        wad_size = size(self.doom1_wad) if self.doom1_wad.exists() else 0
        wad_secs = sectors(wad_size)
        wad_lba = DOOM_BIN_LBA + doom_secs
        payload_end = DOOM_BIN_LBA + doom_secs
        if wad_secs > 0:
            payload_end = max(payload_end, wad_lba + wad_secs)
        posix_lba = payload_end
        tls_lba = posix_lba + sectors(size(posix))
        initrd_end = tls_lba + sectors(size(tls))
        initrd_secs = initrd_end - DOOM_BIN_LBA if initrd_end > DOOM_BIN_LBA else 0
        return {
            "doom_bin": doom_bin,
            "doom_size": doom_size,
            "doom_secs": doom_secs,
            "wad": self.doom1_wad,
            "wad_size": wad_size,
            "wad_lba": wad_lba,
            "posix": posix,
            "posix_size": size(posix),
            "posix_lba": posix_lba,
            "tls": tls,
            "tls_size": size(tls),
            "tls_lba": tls_lba,
            "initrd_secs": initrd_secs,
            "initrd_size": initrd_secs * 512,
        }

    def disk_payload_cflags(self, arch):
        layout = self.payload_layout(arch)
        flags = [
            "-DNARCOS_DISK_DOOM_BIN=1",
            f"-DNARCOS_DISK_DOOM_BIN_LBA={DOOM_BIN_LBA}",
            f"-DNARCOS_DISK_DOOM_BIN_SIZE={layout['doom_size']}",
            f"-DNARCOS_DISK_INITRD_LBA={DOOM_BIN_LBA}",
            f"-DNARCOS_DISK_INITRD_SIZE={layout['initrd_size']}",
            f"-DNARCOS_DISK_INITRD_ADDR={INITRD_ADDR}",
            "-DNARCOS_DISK_POSIX_SMOKE=1",
            f"-DNARCOS_DISK_POSIX_SMOKE_LBA={layout['posix_lba']}",
            f"-DNARCOS_DISK_POSIX_SMOKE_SIZE={layout['posix_size']}",
            "-DNARCOS_DISK_TLS_TOOLS=1",
            f"-DNARCOS_DISK_TLS_TOOLS_LBA={layout['tls_lba']}",
            f"-DNARCOS_DISK_TLS_TOOLS_SIZE={layout['tls_size']}",
        ]
        if self.doom1_wad.exists():
            flags += [
                "-DNARCOS_DISK_DOOM1_WAD=1",
                f"-DNARCOS_DISK_DOOM1_WAD_LBA={layout['wad_lba']}",
                f"-DNARCOS_DISK_DOOM1_WAD_SIZE={layout['wad_size']}",
            ]
        return flags

    def build_kernel(self, arch, assets, binaries):
        cfg = self.arch_cfg(arch)
        embed_objects = []
        for program in USER_EMBED_PROGRAMS:
            obj = self.obj_dir(arch) / f"user/embed/{program}.o"
            self.link_binary_object(arch, binaries[program], obj)
            embed_objects.append(obj)

        kernel_objects = []
        for source in self.source_list(arch, "asm"):
            obj = self.obj_for_source(arch, source)
            self.assemble(arch, source, obj)
            kernel_objects.append(obj)
        for source in self.source_list(arch, "c"):
            obj = self.obj_for_source(arch, source)
            source_rel = rel(source.relative_to(self.src))
            extra = self.disk_payload_cflags(arch) if source_rel == "kernel/fs/fs.c" else None
            deps = self.kernel_headers
            if source_rel == "kernel/fs/fs.c":
                deps = deps + [binaries["doom"], binaries["posix_smoke"], binaries["tls_tools"]]
                if self.doom1_wad.exists():
                    deps.append(self.doom1_wad)
            self.compile_c(arch, source, obj, self.kernel_cflags(arch, extra=extra), deps=deps)
            kernel_objects.append(obj)

        all_objects = kernel_objects + embed_objects + [assets["kernel_bg"], assets["kernel_logo"]]
        inputs = all_objects + [cfg["linker"]]
        if newer(cfg["kernel_elf"], inputs):
            cfg["kernel_elf"].parent.mkdir(parents=True, exist_ok=True)
            object_args = [Path(obj).resolve().relative_to(self.build) for obj in all_objects]
            run([self.ld, *cfg["kernel_ldflags"], "-o", cfg["kernel_elf"], *object_args], cwd=self.build)
            label = "kernel ELF" if arch == "i386" else "experimental kernel"
            print(f"[OK] {arch} {label}: {cfg['kernel_elf']} ({size(cfg['kernel_elf'])} byte)")
        if newer(cfg["kernel_bin"], [cfg["kernel_elf"]]):
            run([self.objcopy, "-O", "binary", cfg["kernel_elf"], cfg["kernel_bin"]])
            print(f"[OK] {arch} raw kernel: {cfg['kernel_bin']} ({size(cfg['kernel_bin'])} byte)")

    def build_boot(self, arch):
        cfg = self.arch_cfg(arch)
        stage2 = self.obj_dir(arch) / "boot/stage2.bin"
        boot = self.obj_dir(arch) / "boot/boot.bin"
        manifest = self.obj_dir(arch) / "boot/manifest.bin"
        kernel_secs = sectors(cfg["kernel_elf"])
        stage2.parent.mkdir(parents=True, exist_ok=True)
        if newer(stage2, [cfg["stage2_src"], cfg["kernel_elf"]]):
            self.assemble(arch, cfg["stage2_src"], stage2, fmt="bin", defines={
                "KERNEL_SECTORS": kernel_secs,
                "BOOT_MANIFEST_LBA": BOOT_MANIFEST_LBA,
                "VBE_WIDTH": self.vbe_width,
                "VBE_HEIGHT": self.vbe_height,
            })
        stage2_secs = sectors(stage2)
        if stage2_secs > 16:
            raise SystemExit(f"[ERR] {arch} stage2 too large for manifest layout: {stage2_secs} sectors > 16")
        if newer(boot, [self.src / "boot/boot.asm", stage2]):
            self.assemble(arch, self.src / "boot/boot.asm", boot, fmt="bin", defines={
                "STAGE2_SECTORS": stage2_secs,
                "DISK_IMAGE_SECTORS": self.disk_image_sectors,
            })
        layout = self.payload_layout(arch)
        if newer(manifest, [cfg["kernel_elf"], layout["doom_bin"], layout["tls"], self.src / "tools/make_boot_manifest.py"]):
            run([
                sys.executable, self.src / "tools/make_boot_manifest.py",
                cfg["kernel_elf"], manifest, KERNEL_START_LBA,
                "--initrd-lba", DOOM_BIN_LBA,
                "--initrd-sectors", layout["initrd_secs"],
                "--initrd-size", layout["initrd_size"],
            ])
            print(f"[OK] {arch} boot manifest: {manifest}")
        return boot, stage2, manifest

    def build_image(self, arch):
        assets = self.build_asset_objects(arch)
        binaries = self.build_user_objects(arch, assets)
        self.build_kernel(arch, assets, binaries)
        boot, stage2, manifest = self.build_boot(arch)
        cfg = self.arch_cfg(arch)
        layout = self.payload_layout(arch)
        image = cfg["image"]
        inputs = [
            boot, stage2, manifest, cfg["kernel_elf"], *binaries.values(),
            self.src / "tools/meson_build.py",
            self.src / "tools/seed_narcos_fs.py",
        ]
        if self.doom1_wad.exists():
            inputs.append(self.doom1_wad)
        if not newer(image, inputs):
            return image
        image.parent.mkdir(parents=True, exist_ok=True)
        with image.open("wb") as f:
            f.truncate(self.disk_image_sectors * 512)

        def write_at(path, lba):
            with Path(path).open("rb") as src_f, image.open("r+b") as dst_f:
                dst_f.seek(lba * 512)
                shutil.copyfileobj(src_f, dst_f)

        print(f"[INFO] {arch} kernel sector size: {sectors(cfg['kernel_elf'])}")
        if layout["doom_size"] > DOOM_BIN_MAX_SIZE:
            raise SystemExit(f"[ERR] {arch} doom binary too large for payload slot: {layout['doom_size']} > {DOOM_BIN_MAX_SIZE}")
        if self.doom1_wad.exists() and layout["wad_size"] > DOOM1_WAD_MAX_SIZE:
            raise SystemExit(f"[ERR] {self.doom1_wad} too large for payload slot: {layout['wad_size']} > {DOOM1_WAD_MAX_SIZE}")
        write_at(boot, 0)
        write_at(stage2, 1)
        write_at(manifest, BOOT_MANIFEST_LBA)
        write_at(cfg["kernel_elf"], KERNEL_START_LBA)
        write_at(layout["doom_bin"], DOOM_BIN_LBA)
        if self.doom1_wad.exists():
            write_at(self.doom1_wad, layout["wad_lba"])
        write_at(layout["posix"], layout["posix_lba"])
        write_at(layout["tls"], layout["tls_lba"])
        run([
            sys.executable, self.src / "tools/seed_narcos_fs.py", image,
            "--bin-dir", self.obj_dir(arch) / "user/bin",
            "--programs", " ".join(FS_SEED_PROGRAMS),
        ])
        print(f"[OK] {arch} image: {image}")
        return image

    def build_iso(self, arch):
        cfg = self.arch_cfg(arch)
        image = self.build_image(arch)
        iso = cfg["iso"]
        if shutil.which("genisoimage") is None:
            raise SystemExit("[ERR] genisoimage is required to build ISO images")
        if not newer(iso, [image]):
            return iso
        iso_root = self.obj_dir(arch) / "iso"
        if iso_root.exists():
            shutil.rmtree(iso_root)
        (iso_root / "boot").mkdir(parents=True)
        shutil.copy2(image, iso_root / "boot" / cfg["iso_boot_img"])
        (iso_root / "README.TXT").write_text(cfg["iso_readme"], encoding="ascii")
        run([
            "genisoimage", "-quiet", "-V", cfg["iso_volume"], "-b",
            "boot/" + cfg["iso_boot_img"], "-c", "boot/boot.cat",
            "-hard-disk-boot", "-o", iso, iso_root,
        ])
        run([
            sys.executable, self.src / "tools/make_rufus_hybrid.py",
            "--iso", iso, "--boot-asm", self.src / "boot/boot.asm",
            "--disk-image-sectors", self.disk_image_sectors,
            "--nasm", self.as_,
        ])
        print(f"[OK] {arch} Rufus/DD hybrid ISO: {iso}")
        return iso

    def build_usb(self, arch):
        cfg = self.arch_cfg(arch)
        image = self.build_image(arch)
        usb = cfg["usb"]
        if newer(usb, [image]):
            shutil.copy2(image, usb)
            print(f"[OK] {arch} USB/raw image: {usb}")
        return usb

    def export_i386(self):
        self.build_image("i386")
        cfg = self.arch_cfg("i386")
        boot_dir = self.src / "boot"
        shutil.copy2(self.obj_dir("i386") / "boot/boot.bin", boot_dir / "boot.bin")
        shutil.copy2(self.obj_dir("i386") / "boot/stage2.bin", boot_dir / "stage2.bin")
        shutil.copy2(self.obj_dir("i386") / "boot/manifest.bin", boot_dir / "manifest.bin")
        shutil.copy2(cfg["kernel_bin"], self.src / "kernel.bin")
        shutil.copy2(cfg["kernel_elf"], self.src / "kernel.elf")
        shutil.copy2(cfg["image"], self.src / "minios.img")

    def clean_musl(self):
        for arch in ("i386", "x86_64"):
            paths = self.musl_paths(arch)
            if paths["build"].exists():
                subprocess.run(["make", "-C", str(paths["build"]), "clean"], check=False)
            shutil.rmtree(paths["build"], ignore_errors=True)
            shutil.rmtree(paths["dest"], ignore_errors=True)

    def clean_generated(self):
        shutil.rmtree(self.build / "obj", ignore_errors=True)
        shutil.rmtree(self.src / "tools/__pycache__", ignore_errors=True)
        for path in [
            self.src / "boot/boot.bin",
            self.src / "boot/stage2.bin",
            self.src / "boot/manifest.bin",
            self.src / "kernel.bin",
            self.src / "kernel.elf",
            self.src / "kernel64.elf",
            self.src / "kernel64.bin",
            self.src / "kernel.tmp",
            self.src / "minios.img",
        ]:
            try:
                path.unlink()
            except FileNotFoundError:
                pass

    def run_qemu(self, name):
        mapping = {
            "run-i386": ("i386", "image", []),
            "run-net-i386": ("i386", "image", ["-netdev", "user,id=n0", "-device", "rtl8139,netdev=n0"]),
            "run-iso-i386": ("i386", "iso", ["-cdrom", None, "-boot", "d"]),
            "run-iso-usb-i386": ("i386", "iso-drive", []),
            "run-x86_64-headless": ("x86_64", "image", ["-display", "none"]),
            "run-x86_64": ("x86_64", "image", ["-display", "none"]),
            "run-x86_64-gui": ("x86_64", "image", []),
            "run-x86_64-net": ("x86_64", "image", ["-netdev", "user,id=n0", "-device", "rtl8139,netdev=n0"]),
            "run-net": ("x86_64", "image", ["-netdev", "user,id=n0", "-device", "rtl8139,netdev=n0"]),
            "run-iso-x86_64": ("x86_64", "iso", ["-cdrom", None, "-boot", "d", "-display", "none"]),
            "run-iso-usb-x86_64": ("x86_64", "iso-drive", ["-display", "none"]),
        }
        arch, mode, extra = mapping[name]
        cfg = self.arch_cfg(arch)
        if mode == "iso":
            artifact = self.build_iso(arch)
            cmd = [cfg["qemu"], "-m", "128M"]
            for arg in extra:
                cmd.append(str(artifact) if arg is None else arg)
        elif mode == "iso-drive":
            artifact = self.build_iso(arch)
            cmd = [cfg["qemu"], "-m", "128M", "-drive", f"format=raw,file={artifact}", *extra]
        else:
            artifact = self.build_image(arch)
            cmd = [cfg["qemu"], "-m", "128M", "-drive", f"format=raw,file={artifact}", *extra]
        cmd += ["-serial", "stdio", "-no-reboot", "-no-shutdown"]
        run(cmd)

    def dispatch(self, target):
        if target == "all-i386":
            self.build_image("i386")
        elif target == "all-x86_64":
            self.build_image("x86_64")
        elif target == "iso":
            self.build_iso("i386")
        elif target == "iso-i386":
            self.build_iso("i386")
        elif target == "iso-x86_64":
            self.build_iso("x86_64")
        elif target == "usb":
            self.build_usb("i386")
        elif target == "usb-i386":
            self.build_usb("i386")
        elif target == "usb-x86_64":
            self.build_usb("x86_64")
        elif target == "user-programs":
            self.build_user_objects("i386", self.build_asset_objects("i386"))
        elif target == "user-programs-i386":
            self.build_user_objects("i386", self.build_asset_objects("i386"))
        elif target == "user-programs-x86_64":
            self.build_user_objects("x86_64", self.build_asset_objects("x86_64"))
        elif target == "musl-overlay":
            self.apply_musl_overlay()
        elif target == "musl-i386":
            self.build_musl("i386")
        elif target == "musl-x86_64":
            self.build_musl("x86_64")
        elif target == "musl":
            self.build_musl("i386")
            self.build_musl("x86_64")
        elif target == "musl-clean":
            self.clean_musl()
        elif target == "export-i386-artifacts":
            self.export_i386()
        elif target in {
            "run-i386", "run-net-i386", "run-iso-i386", "run-iso-usb-i386",
            "run-x86_64", "run-x86_64-headless", "run-x86_64-gui",
            "run-x86_64-net", "run-net", "run-iso-x86_64",
            "run-iso-usb-x86_64",
        }:
            self.run_qemu(target)
        elif target == "clean-generated":
            self.clean_generated()
        else:
            raise SystemExit(f"unknown target: {target}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--build-root", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--stamp")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--ld", default="ld")
    parser.add_argument("--nasm", default="nasm")
    parser.add_argument("--objcopy", default="objcopy")
    parser.add_argument("--vbe-width", type=int, default=1024)
    parser.add_argument("--vbe-height", type=int, default=768)
    parser.add_argument("--disk-image-sectors", type=int, default=49152)
    parser.add_argument("--musl-dir", default="user/ports/musl")
    parser.add_argument("--musl-prefix", default="/usr")
    parser.add_argument("--autorun-posix-smoke", action="store_true")
    args = parser.parse_args()
    Build(args).dispatch(args.target)
    if args.stamp:
        stamp = Path(args.stamp)
        stamp.parent.mkdir(parents=True, exist_ok=True)
        stamp.write_text(args.target + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
