dxbc_src = files([
  'dxbc_chunk_isgn.cpp',
  'dxbc_chunk_shex.cpp',
  'dxbc_common.cpp',
  'dxbc_compiler.cpp',
  'dxbc_decoder.cpp',
  'dxbc_header.cpp',
  'dxbc_module.cpp',
  'dxbc_names.cpp',
  'dxbc_reader.cpp',
  'dxbc_type.cpp',
  'dxbc_util.cpp',
  
  'gen/dxbc_gen_common.cpp',
  'gen/dxbc_gen_pixel.cpp',
  'gen/dxbc_gen_vertex.cpp',
])

dxbc_lib = static_library('dxbc', dxbc_src,
  include_directories : [ dxvk_include_path ])

dxbc_dep = declare_dependency(
  link_with           : [ dxbc_lib ],
  include_directories : [ dxvk_include_path, include_directories('.') ])
