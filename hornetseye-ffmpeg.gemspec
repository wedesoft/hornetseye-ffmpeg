Gem::Specification.new do |s|
    s.name = 'hornetseye-ffmpeg'
    s.version = '1.1.3'
    s.date = Date.today.to_s
    s.summary = %q{Read/write video frames using libffmpeg}
    s.description = %q{This Ruby extension defines the class Hornetseye::AVInput for reading frames from video files and the class Hornetseye::AVOutput for writing frames to video files.}
    s.author = %q{Jan Wedekind}
    s.email = %q{jan@wedesoft.de}
    s.homepage = %q{http://wedesoft.github.com/hornetseye-ffmpeg/}
    s.files = ['Rakefile', 'README.md', 'COPYING', '.document' ] + Dir.glob("ext/**/*.{cc,hh,tcc,rb}") + Dir.glob("lib/**/*.rb") +  Dir.glob("test/{tc,ts}_*.rb")
    s.test_files = Dir.glob("test/tc_*.rb")
    s.require_paths = [ 'lib', 'ext' ]
    s.rubyforge_project = %q{hornetseye}
    s.extensions = %w{ext/hornetseye-ffmpeg/extconf.rb}
    s.has_rdoc = 'yard'
    s.extra_rdoc_files = []
    s.rdoc_options = %w{--no-private}
    s.add_dependency %<malloc>, [ '~> 1.2' ]
    s.add_dependency %<multiarray>, [ '~> 1.0' ]
    s.add_dependency %<hornetseye-frame>, [ '~> 1.0' ]
    s.add_development_dependency %q{rake}
    s.add_development_dependency "rake-compiler"
  end

