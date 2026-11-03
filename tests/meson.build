subdir('dx11')
metal4_lazy_init_test = executable('metal4_lazy_init', 'metal4_lazy_init.cpp',
  native: true,
  include_directories: include_directories('../src/dxmt', '../src/winemetal'),
  dependencies: dependency('threads', native: true),
)
test('metal4_lazy_init', metal4_lazy_init_test)
