project('dedup', 'c',
  version:          '0.9.16',
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

deps          = []
inc           = []
pkg_sources = files(
  'generic/dedup.c',
)

teaconf_files = files(
  'generic/dedup.h.in'
)

# tea_genstubs: 1st: .decls, 2nd: stublib.c
tea_genstubs = files('generic/dedup.decls', 'generic/dedupStubLib.c')

subdir('teabase/init')

conf.set_quoted('DEDUP_LIBRARY_PATH_INSTALL', pkg_dir)
conf.set_quoted('DEDUP_INCLUDE_PATH_INSTALL', pkg_dir)
conf.set_quoted('DEDUP_PACKAGE_PATH_INSTALL', pkg_dir)
if tclver_major.to_int() >= 9
  conf.set_quoted('DEDUP_LIBRARY', 'tcl9' + meson.project_name())
else
  conf.set_quoted('DEDUP_LIBRARY', meson.project_name())
endif
conf.set_quoted('DEDUP_STUBLIB', meson.project_name() + 'stub')
conf.set_quoted('DEDUP_HEADER', 'dedup.h')
conf.set_quoted('DEDUP_INCLUDE_SPEC', '-I' + pkg_dir)
conf.set_quoted('DEDUP_BUILD_INCLUDE_SPEC', '-I' + meson.project_build_root())
conf.set_quoted('DEDUP_LIB_SPEC', '-L' + pkg_dir)
conf.set_quoted('DEDUP_BUILD_LIB_SPEC', '-L' + meson.project_build_root() / 'teabase')
conf.set_quoted('DEDUP_STUB_LIB_SPEC', '-L' + pkg_dir)
conf.set_quoted('DEDUP_BUILD_STUB_LIB_SPEC', '-L' + meson.project_build_root() / 'teabase')

subdir('teabase')

# Generate relocatable pkg-config file
pc_conf = configuration_data()
pc_conf.set('PACKAGE_NAME', meson.project_name())
pc_conf.set('PACKAGE_VERSION', meson.project_version())
pc_conf.set('PKG_DIR_BASENAME', meson.project_name() + '-' + meson.project_version())
pc_conf.set('STUBLIB', meson.project_name() + 'stub')

configure_file(
  input:          meson.project_name() + '.pc.in',
  output:         meson.project_name() + '.pc',
  configuration:  pc_conf,
  install:        not meson.is_subproject(),
  install_dir:    tcl_dep.get_variable('libdir') / 'pkgconfig',
)

test_conf = configuration_data()
if tclver_major.to_int() >= 9
  test_conf.set('DEDUP_LIBRARY', 'tcl9' + meson.project_name())
else
  test_conf.set('DEDUP_LIBRARY', meson.project_name())
endif
test_conf.set('DEDUP_STUBLIB', meson.project_name() + 'stub')

configure_file(
  input:          'tests/test_config.tcl.in',
  output:         'test_config.tcl',
  configuration:  test_conf,
)

dedup_dep = declare_dependency(
  variables: {
    'tcl_pkg_path': fs.parent(pkg_so.full_path()),
    'version':      meson.project_version(),
  },
)
meson.override_dependency(meson.project_name(), dedup_dep)

subdir('doc')
