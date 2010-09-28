#!/usr/bin/env ruby
require 'date'
require 'rake/clean'
require 'rake/testtask'
require 'rake/packagetask'
require 'rbconfig'

PKG_NAME = 'hornetseye-ffmpeg'
PKG_VERSION = '0.2.0'
CXX = ENV[ 'CXX' ] || 'g++'
STRIP = ENV[ 'STRIP' ] || 'strip'
RB_FILES = FileList[ 'lib/**/*.rb' ]
CC_FILES = FileList[ 'ext/*.cc' ]
HH_FILES = FileList[ 'ext/*.hh' ]
TC_FILES = FileList[ 'test/tc_*.rb' ]
TS_FILES = FileList[ 'test/ts_*.rb' ]
SO_FILE = "ext/#{PKG_NAME.tr '\-', '_'}.so"
PKG_FILES = [ 'Rakefile', 'README.md', 'COPYING', '.document' ] +
            RB_FILES + CC_FILES + HH_FILES + TS_FILES + TC_FILES
BIN_FILES = [ 'README.md', 'COPYING', '.document', SO_FILE ] +
            RB_FILES + TS_FILES + TC_FILES
SUMMARY = %q{Read video frames using libffmpeg}
DESCRIPTION = %q{This Ruby extension defines the class Hornetseye::AVInput for reading frames from video files.}
AUTHOR = %q{Jan Wedekind}
EMAIL = %q{jan@wedesoft.de}
HOMEPAGE = %q{http://wedesoft.github.com/hornetseye-ffmpeg/}

OBJ = CC_FILES.ext 'o'
$CXXFLAGS = ENV[ 'CXXFLAGS' ] || ''
$CXXFLAGS = "#{$CXXFLAGS} -fPIC"
if Config::CONFIG[ 'rubyhdrdir' ]
  $CXXFLAGS += "#{$CXXFLAGS} -I#{Config::CONFIG[ 'rubyhdrdir' ]} " +
              "-I#{Config::CONFIG[ 'rubyhdrdir' ]}/#{Config::CONFIG[ 'arch' ]}"
else
  $CXXFLAGS += "#{$CXXFLAGS} -I#{Config::CONFIG[ 'archdir' ]}"
end
$LIBRUBYARG = Config::CONFIG[ 'LIBRUBYARG' ]
$SITELIBDIR = Config::CONFIG[ 'sitelibdir' ]
$SITEARCHDIR = Config::CONFIG[ 'sitearchdir' ]

task :default => :all

desc 'Compile Ruby extension (default)'
task :all => [ SO_FILE ]

file SO_FILE => OBJ do |t|
   sh "#{CXX} -shared -o #{t.name} #{OBJ} -lavformat -lswscale #{$LIBRUBYARG}"
   # sh "#{STRIP} --strip-all #{t.name}"
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

Rake::TestTask.new do |t|
  t.libs << 'ext'
  t.test_files = TC_FILES
end

begin
  require 'yard'
  YARD::Rake::YardocTask.new :yard do |y|
    y.options << '--no-private'
    y.files << FileList[ 'lib/**/*.rb' ]
  end
rescue LoadError
  STDERR.puts 'Please install \'yard\' if you want to generate documentation'
end

Rake::PackageTask.new PKG_NAME, PKG_VERSION do |p|
  p.need_tar = true
  p.package_files = PKG_FILES
end

begin
  require 'rubygems/builder'
  $SPEC = Gem::Specification.new do |s|
    s.name = PKG_NAME
    s.version = PKG_VERSION
    s.platform = Gem::Platform::RUBY
    s.date = Date.today.to_s
    s.summary = SUMMARY
    s.description = DESCRIPTION
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
    s.add_dependency %<malloc>, [ '~> 1.1' ]
    s.add_dependency %<multiarray>, [ '~> 0.5' ]
    s.add_dependency %<hornetseye-frame>, [ '~> 0.1' ]
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
    s.add_dependency %<malloc>, [ '~> 1.1' ]
    s.add_dependency %<multiarray>, [ '~> 0.5' ]
    s.add_dependency %<hornetseye-frame>, [ '~> 0.1' ]
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

file 'ext/avinput.o' => [ 'ext/avinput.cc', 'ext/avinput.hh', 'ext/error.hh',
                          'ext/frame.hh' ]
file 'ext/frame.o' => [ 'ext/frame.cc', 'ext/frame.hh' ]
file 'ext/init.o' => [ 'ext/init.cc', 'ext/avinput.hh' ]

CLEAN.include 'ext/*.o'
CLOBBER.include SO_FILE, 'doc', '.yardoc'

