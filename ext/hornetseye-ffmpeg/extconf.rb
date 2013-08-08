require 'mkmf'
extension_name = 'hornetseye_ffmpeg'

def exit_failure(msg)
    Logging::message msg
    message msg+"\n"
    exit(1)
end

config = defined?(RbConfig) ? ::RbConfig : ::Config
CFG = config::CONFIG

CXX = (ENV["CXX"] or config::CONFIG["CXX"] or "g++").split(' ').first
unless find_executable(CXX)
    exit_failure "No C++ compiler found in ${ENV['PATH']}. See mkmf.log for details."
end

$defs.push("-DNDEBUG")
$defs.push("-D__STDC_CONSTANT_MACROS")
$CXXFLAGS = "#{CFG[ 'CPPFLAGS' ]} #{CFG[ 'CFLAGS' ]}"
if CFG[ 'rubyhdrdir' ]
  $CXXFLAGS = "#{$CXXFLAGS} -I#{CFG[ 'rubyhdrdir' ]} " + 
              "-I#{CFG[ 'rubyhdrdir' ]}/#{CFG[ 'arch' ]}"
else
  $CXXFLAGS = "#{$CXXFLAGS} -I#{CFG[ 'archdir' ]}"
end
$LDFLAGS = "-L#{CFG[ 'libdir' ]} #{CFG[ 'LIBRUBYARG' ]} #{CFG[ 'LDFLAGS' ]} #{CFG[ 'SOLIBS' ]} #{CFG[ 'DLDLIBS' ]}"
$LOCAL_LIBS = "-lavformat -lavcodec -lavutil -lswscale"

if have_header("libswscale/swscale.h")
    $defs.push("-DHAVE_LIBSWSCALE_INCDIR=1")
elsif have_header("ffmpeg/swscale.h")
else
    exit_failure 'Cannot find swscale.h header file'
end

if have_header("libavformat/avformat.h")
    $defs.push("-DHAVE_LIBAVFORMAT_INCDIR=1")
elsif have_header("ffmpeg/avformat.h")
else
    exit_failure 'Cannot find avformat.h header file'
end

dir_config(extension_name)
create_makefile(extension_name)
