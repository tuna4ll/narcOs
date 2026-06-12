project('NarcOs', 'c', version: '0.1.0')

python = import('python').find_installation('python3')
build_tool = files('tools/meson_build.py')

common_args = [
  '--source-root', meson.project_source_root(),
  '--build-root', meson.project_build_root(),
  '--cc', find_program('gcc').full_path(),
  '--ld', find_program('ld').full_path(),
  '--nasm', find_program('nasm').full_path(),
  '--objcopy', find_program('objcopy').full_path(),
  '--vbe-width', get_option('vbe_width').to_string(),
  '--vbe-height', get_option('vbe_height').to_string(),
  '--disk-image-sectors', get_option('disk_image_sectors').to_string(),
  '--musl-dir', get_option('musl_dir'),
  '--musl-prefix', get_option('musl_prefix'),
]

if get_option('autorun_posix_smoke')
  common_args += ['--autorun-posix-smoke']
endif

meson.add_postconf_script(
  python,
  files('tools/patch_meson_clean.py'),
  common_args,
)

build_targets = [
  'all-i386',
  'all-x86_64',
  'iso',
  'iso-i386',
  'iso-x86_64',
  'usb',
  'usb-i386',
  'usb-x86_64',
  'user-programs',
  'user-programs-i386',
  'user-programs-x86_64',
  'musl-overlay',
  'musl-i386',
  'musl-x86_64',
  'musl',
  'musl-clean',
  'export-i386-artifacts',
]

foreach target_name : build_targets
  target_stamp = custom_target(
    target_name + '-stamp',
    output: target_name + '.stamp',
    command: [
      python, build_tool,
      '--target', target_name,
      '--stamp', '@OUTPUT@',
    ] + common_args,
    build_by_default: target_name == 'all-i386',
    build_always_stale: true,
    console: true,
  )
  alias_target(target_name, target_stamp)
endforeach

run_targets = [
  'run-i386',
  'run-net-i386',
  'run-iso-i386',
  'run-iso-usb-i386',
  'run-x86_64',
  'run-x86_64-headless',
  'run-x86_64-gui',
  'run-x86_64-net',
  'run-net',
  'run-iso-x86_64',
  'run-iso-usb-x86_64',
  'clean-generated',
]

foreach target_name : run_targets
  run_target(
    target_name,
    command: [
      python, build_tool,
      '--target', target_name,
    ] + common_args,
  )
endforeach
