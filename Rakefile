#!/usr/bin/env ruby
require 'date'
require 'rake/clean'
require 'rake/testtask'
require 'rake/packagetask'
require 'rake/loaders/makefile'
require 'rbconfig'

PKG_NAME = 'hornetseye-ffmpeg'
PKG_VERSION = '1.2.1'
CFG = RbConfig::CONFIG
CXX = ENV[ 'CXX' ] || 'g++'
RB_FILES = FileList[ 'lib/**/*.rb' ]
CC_FILES = FileList[ 'ext/*.cc' ]
HH_FILES = FileList[ 'ext/*.hh' ] + FileList[ 'ext/*.tcc' ]
TC_FILES = FileList[ 'test/tc_*.rb' ]
TS_FILES = FileList[ 'test/ts_*.rb' ]
SO_FILE = "ext/#{PKG_NAME.tr '\-', '_'}.#{CFG[ 'DLEXT' ]}"
PKG_FILES = [ 'Rakefile', 'README.md', 'COPYING', '.document' ] +
            RB_FILES + CC_FILES + HH_FILES + TS_FILES + TC_FILES
BIN_FILES = [ 'README.md', 'COPYING', '.document', SO_FILE ] +
            RB_FILES + TS_FILES + TC_FILES
SUMMARY = %q{Read/write video frames using libffmpeg}
DESCRIPTION = %q{This Ruby extension defines the class Hornetseye::AVInput for reading frames from video files and the class Hornetseye::AVOutput for writing frames to video files.}
LICENSE = 'GPL-3+'
AUTHOR = %q{Jan Wedekind}
EMAIL = %q{jan@wedesoft.de}
HOMEPAGE = %q{http://wedesoft.github.com/hornetseye-ffmpeg/}

OBJ = CC_FILES.ext 'o'
$CXXFLAGS = "-DNDEBUG -DHAVE_CONFIG_H -D__STDC_CONSTANT_MACROS #{CFG[ 'CPPFLAGS' ]} #{CFG[ 'CFLAGS' ]}"
if CFG[ 'rubyhdrdir' ]
  $CXXFLAGS = "#{$CXXFLAGS} -I#{CFG[ 'rubyhdrdir' ]} " + 
              "-I#{CFG[ 'rubyhdrdir' ]}/#{CFG[ 'arch' ]}"
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

begin
  require 'rubygems'
  require 'rubygems/builder'
  $SPEC = Gem::Specification.new do |s|
    s.name = PKG_NAME
    s.version = PKG_VERSION
    s.platform = Gem::Platform::RUBY
    s.date = Date.today.to_s
    s.summary = SUMMARY
    s.description = DESCRIPTION
    s.license = LICENSE
    s.author = AUTHOR
    s.email = EMAIL
    s.homepage = HOMEPAGE
    s.files = PKG_FILES
    s.test_files = TC_FILES
    s.require_paths = [ 'lib', 'ext' ]
    s.rubyforge_project = %q{hornetseye}
    s.extensions = %w{Rakefile}
    s.has_rdoc = 'yard'
    s.extra_rdoc_files = []
    s.rdoc_options = %w{--no-private}
    s.add_dependency %<malloc>, [ '~> 1.2' ]
    s.add_dependency %<multiarray>, [ '~> 1.0' ]
    s.add_dependency %<hornetseye-frame>, [ '~> 1.0' ]
    s.add_development_dependency %q{rake}
  end
  GEM_SOURCE = "#{PKG_NAME}-#{PKG_VERSION}.gem"
  $BINSPEC = Gem::Specification.new do |s|
    s.name = PKG_NAME
    s.version = PKG_VERSION
    s.platform = Gem::Platform::CURRENT
    s.date = Date.today.to_s
    s.summary = SUMMARY
    s.description = DESCRIPTION
    s.license = LICENSE
    s.author = AUTHOR
    s.email = EMAIL
    s.homepage = HOMEPAGE
    s.files = BIN_FILES
    s.test_files = TC_FILES
    s.require_paths = [ 'lib', 'ext' ]
    s.rubyforge_project = %q{hornetseye}
    s.has_rdoc = 'yard'
    s.extra_rdoc_files = []
    s.rdoc_options = %w{--no-private}
    s.add_dependency %<malloc>, [ '~> 1.2' ]
    s.add_dependency %<multiarray>, [ '~> 1.0' ]
    s.add_dependency %<hornetseye-frame>, [ '~> 1.0' ]
  end
  GEM_BINARY = "#{PKG_NAME}-#{PKG_VERSION}-#{$BINSPEC.platform}.gem"
  desc "Build the gem file #{GEM_SOURCE}"
  task :gem => [ "pkg/#{GEM_SOURCE}" ]
  file "pkg/#{GEM_SOURCE}" => [ 'pkg' ] + $SPEC.files do
    when_writing 'Creating GEM' do
      Gem::Builder.new( $SPEC ).build
      verbose true do
        FileUtils.mv GEM_SOURCE, "pkg/#{GEM_SOURCE}"
      end
    end
  end
  desc "Build the gem file #{GEM_BINARY}"
  task :gem_binary => [ "pkg/#{GEM_BINARY}" ]
  file "pkg/#{GEM_BINARY}" => [ 'pkg' ] + $BINSPEC.files do
    when_writing 'Creating binary GEM' do
      Gem::Builder.new( $BINSPEC ).build
      verbose true do
        FileUtils.mv GEM_BINARY, "pkg/#{GEM_BINARY}"
      end
    end
  end
rescue LoadError
  STDERR.puts 'Please install \'rubygems\' if you want to create Gem packages'
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

