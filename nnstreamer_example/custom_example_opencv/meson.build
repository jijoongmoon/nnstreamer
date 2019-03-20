opencv_dep = dependency('opencv')

library('nnstreamer_customfilter_opencv_scaler',
  'nnstreamer_customfilter_opencv_scaler.cc',
  dependencies: [glib_dep, opencv_dep],
  include_directories: nnstreamer_inc,
  install: get_option('install-example'),
  install_dir: join_paths(unittest_install_dir, 'nnstreamer_example','custom_example_opencv')
)

library('nnstreamer_customfilter_opencv_average',
  'nnstreamer_customfilter_opencv_average.cc',
  dependencies: [glib_dep, opencv_dep],
  include_directories: nnstreamer_inc,
  install: get_option('install-example'),
  install_dir: join_paths(unittest_install_dir, 'nnstreamer_example','custom_example_opencv')
)
