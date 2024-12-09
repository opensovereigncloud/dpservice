dpservice_inspect_sources = [
  'inspect.c',
  'inspect_conntrack.c',
  'inspect_lb.c',
  'main.c',
  '../../src/dp_argparse.c',
  '../../src/dp_error.c',
  '../../src/dp_ipaddr.c',
]

executable('dpservice-inspect',
  sources: [ dpservice_inspect_sources, version_h ],
  include_directories: [includes],
  dependencies: [dpdk_dep] )
