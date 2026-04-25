test_exe = executable(
  'test_ppack',
  ['test_ppack.c'],
  dependencies: [ppack_dep],
  include_directories: public_headers,
)

test('ppack 8-bit native', test_exe)

test_16bit_exe = executable(
  'test_ppack_16bit',
  ['test_ppack.c'],
  dependencies: [ppack_sim_dep],
  include_directories: public_headers,
)

test('ppack 16-bit simulated', test_16bit_exe)
