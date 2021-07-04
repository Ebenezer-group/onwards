project('onwards','cpp')
add_global_arguments('-std=c++17','-Os','-flto','-W','-Wall','-Wextra',language:'cpp')
inc=include_directories('src')
executable('genz','src/cmw/tiers/front/genz.cc',include_directories:inc)
message(host_machine.system())
if host_machine.system()!='WIN32'
  executable('cmwA','src/cmw/tiers/cmwA.cc',include_directories:inc)
endif
executable('sendExample','example/sendExample.cc',include_directories:inc)
executable('receiveExample','example/receiveExample.cc',include_directories:inc)
dest=join_paths(meson.build_root(),'genz')
r=run_command('size',dest)

