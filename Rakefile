#!/usr/bin/env ruby
require 'date'
require 'rake/clean'
require 'rake/testtask'
require 'rake/packagetask'
require 'rake/loaders/makefile'
require 'rbconfig'
require_relative 'config'

OBJ = CC_FILES.ext 'o'
$CXXFLAGS = "-DNDEBUG -DHAVE_CONFIG_H -D__STDC_CONSTANT_MACROS #{CFG[ 'CPPFLAGS' ]} #{CFG[ 'CFLAGS' ]}"
if CFG['rubyarchhdrdir']
  $CXXFLAGS = "#{$CXXFLAGS} -I#{CFG['rubyhdrdir']} -I#{CFG['rubyarchhdrdir']}"
elsif CFG['rubyhdrdir']
  $CXXFLAGS = "#{$CXXFLAGS} -I#{CFG['rubyhdrdir' ]} -I#{CFG['rubyhdrdir']}/#{CFG['arch']}"
else
  $CXXFLAGS = "#{$CXXFLAGS} -I#{CFG[ 'archdir' ]}"
end
$LIBRUBYARG = "-L#{CFG[ 'libdir' ]} #{CFG[ 'LIBRUBYARG' ]} #{CFG[ 'LDFLAGS' ]} " +
              "#{CFG[ 'SOLIBS' ]} #{CFG[ 'DLDLIBS' ]}"
$SITELIBDIR = CFG[ 'sitelibdir' ]
$SITEARCHDIR = CFG[ 'sitearchdir' ]
$LDSHARED = CFG[ 'LDSHARED' ][ CFG[ 'LDSHARED' ].index( ' ' ) .. -1 ]
$CXXFLAGS = "#{$CXXFLAGS} -I/usr/include/ffmpeg"

task :default => :all

desc 'Compile Ruby extension (default)'
task :all => [ SO_FILE ]

file SO_FILE => OBJ do |t|
   sh "#{CXX} -shared -o #{t.name} #{OBJ} -lavformat -lavcodec -lavutil -lswscale #{$LIBRUBYARG}"
end

task :test => [ SO_FILE ]

desc 'Install Ruby extension'
task :install => :all do
  verbose true do
    for f in RB_FILES do
      FileUtils.mkdir_p "#{$SITELIBDIR}/#{File.dirname f.gsub( /^lib\//, '' )}"
      FileUtils.cp_r f, "#{$SITELIBDIR}/#{f.gsub( /^lib\//, '' )}"
    end
    FileUtils.mkdir_p $SITEARCHDIR
    FileUtils.cp SO_FILE, "#{$SITEARCHDIR}/#{File.basename SO_FILE}"
  end
end

desc 'Uninstall Ruby extension'
task :uninstall do
  verbose true do
    for f in RB_FILES do
      FileUtils.rm_f "#{$SITELIBDIR}/#{f.gsub /^lib\//, ''}"
    end
    FileUtils.rm_f "#{$SITEARCHDIR}/#{File.basename SO_FILE}"
  end
end

desc 'Create config.h'
task :config_h => 'ext/config.h'

def check_program
  f_base_name = 'rakeconf'
  begin
    File.open( "#{f_base_name}.cc", 'w' ) { |f| yield f }
    `#{CXX} #{$CXXFLAGS} -c -o #{f_base_name}.o #{f_base_name}.cc 2>&1 >> rake.log`
    $?.exitstatus == 0
  ensure
    File.delete *Dir.glob( "#{f_base_name}.*" )
  end
end

def check_c_header( name )
  check_program do |c|
    c.puts <<EOS
extern "C" {
  #include <#{name}>
}
int main(void) { return 0; }
EOS
  end
end

file 'ext/config.h' do |t|
  s = "/* config.h. Generated from Rakefile by rake. */\n"
  # need to compile with -D__STDC_CONSTANT_MACROS
  if check_c_header 'libswscale/swscale.h'
    s << "#define HAVE_LIBSWSCALE_INCDIR 1\n"
  else
    unless check_c_header 'ffmpeg/swscale.h'
      raise 'Cannot find swscale.h header file'
    end
    s << "#undef HAVE_LIBSWSCALE_INCDIR\n"
  end
  have_libavformat_incdir = check_c_header 'libavformat/avformat.h'
  if have_libavformat_incdir
    s << "#define HAVE_LIBAVFORMAT_INCDIR 1\n"
  elsif check_c_header 'ffmpeg/avformat.h'
    s << "#undef HAVE_LIBAVFORMAT_INCDIR\n"
  else
    raise 'Cannot find avformat.h header file'
  end
  File.open( t.name, 'w' ) { |f| f.puts s }
end

Rake::TestTask.new do |t|
  t.libs << 'ext'
  t.test_files = TC_FILES
end

begin
  require 'yard'
  YARD::Rake::YardocTask.new :yard do |y|
    y.files << RB_FILES
  end
rescue LoadError
  STDERR.puts 'Please install \'yard\' if you want to generate documentation'
end

Rake::PackageTask.new PKG_NAME, PKG_VERSION do |p|
  p.need_tar = true
  p.package_files = PKG_FILES
end

rule '.o' => '.cc' do |t|
   sh "#{CXX} #{$CXXFLAGS} -c -o #{t.name} #{t.source}"
end

file ".depends.mf" => :config_h do |t|
  sh "g++ -MM #{CC_FILES.join ' '} | " +
    "sed -e :a -e N -e 's/\\n/\\$/g' -e ta | " +
    "sed -e 's/ *\\\\\\$ */ /g' -e 's/\\$/\\n/g' | sed -e 's/^/ext\\//' > #{t.name}"
end
CC_FILES.each do |t|
  file t.ext(".o") => t
end
import ".depends.mf"

CLEAN.include 'ext/*.o'
CLOBBER.include SO_FILE, 'doc', '.yardoc', '.depends.mf', 'ext/config.h'

