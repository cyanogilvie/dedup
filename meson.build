project('dedup', 'c',
  version:          '0.9.10',
  default_options:  ['c_std=c11', 'warning_level=3', 'werror=true'],
  meson_version:    '>=1.3.0',
)

cc = meson.get_compiler('c')
fs = import('fs')
add_project_arguments('-D_DEFAULT_SOURCE', language: 'c')

if get_option('debug')
  add_project_arguments(cc.get_supported_arguments(['-ggdb3']), language: 'c')
endif

add_project_arguments('-Wno-missing-field-initializers', language: 'c')

conf = configuration_data()
conf.set10('UNLOAD',   get_option('unload'))
conf.set10('TESTMODE', get_option('testmode'))
conf.set10('DEBUG',    get_option('debug'))
conf.set10('HAVE_FFSLL',            cc.has_function('ffsll', prefix: '#include <strings.h>'))
conf.set10('HAVE___BUILTIN_FFSLL',  cc.has_function('__builtin_ffsll'))

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

deps = []
inc = []

pkg_sources = files(
  'generic/dedup.c',
)

subdir('teabase/init')

deps += declare_dependency(
  sources: custom_target('genstubs',
    output: ['dedupStubInit.c', 'dedupDecls.h'],
    command: [
      tclsh,
      meson.project_source_root() / 'teabase/tools/genStubs.tcl',
      meson.project_build_root(),
      meson.project_source_root() / 'generic/dedup.decls',
    ],
  ),
)

conf.set_quoted('DEDUP_LIBRARY_PATH_INSTALL', pkg_dir)
conf.set_quoted('DEDUP_INCLUDE_PATH_INSTALL', pkg_dir)
conf.set_quoted('DEDUP_PACKAGE_PATH_INSTALL', pkg_dir)
conf.set_quoted('DEDUP_LIBRARY', meson.project_name())
conf.set_quoted('DEDUP_STUBLIB', meson.project_name() + 'stub')
conf.set_quoted('DEDUP_HEADER', 'dedup.h')
conf.set_quoted('DEDUP_INCLUDE_SPEC', '-I' + pkg_dir)
conf.set_quoted('DEDUP_BUILD_INCLUDE_SPEC', '-I' + meson.project_build_root())
conf.set_quoted('DEDUP_LIB_SPEC', '-L' + pkg_dir)
conf.set_quoted('DEDUP_BUILD_LIB_SPEC', '-L' + meson.project_build_root() / 'teabase')
conf.set_quoted('DEDUP_STUB_LIB_SPEC', '-L' + pkg_dir)
conf.set_quoted('DEDUP_BUILD_STUB_LIB_SPEC', '-L' + meson.project_build_root() / 'teabase')

subdir('teabase')

pkg_sources += configure_file(
  configuration:  teaconf,
  input:          'generic/dedup.h.in',
  output:         '@BASENAME@',
  install_dir:    pkg_dir,
)

configure_file(
  configuration:  conf,
  input:          'dedupConfig.sh.in',
  output:         '@BASENAME@',
  install_dir:    pkg_dir,
)

test_conf = configuration_data()
test_conf.set('DEDUP_LIBRARY', meson.project_name())
test_conf.set('DEDUP_STUBLIB', meson.project_name() + 'stub')

configure_file(
  input:          'tests/test_config.tcl.in',
  output:         'test_config.tcl',
  configuration:  test_conf,
)

subdir('doc')
