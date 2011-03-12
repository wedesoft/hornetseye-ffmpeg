hornetseye-ffmpeg
=================

**Author**:       Jan Wedekind
**Copyright**:    2010, 2011
**License**:      GPL

Synopsis
--------

This Ruby extension defines the class {Hornetseye::AVInput} for reading frames from video files and the class {Hornetseye::AVOutput} for writing frames to video files.

Installation
------------

*hornetseye-ffmpeg* requires FFMpeg and the software scaling library. If you are running Debian or (K)ubuntu, you can install them like this:

    $ sudo aptitude install libavformat-dev libswscale-dev

To install this Ruby extension, use the following command:

    $ sudo gem install hornetseye-ffmpeg

Alternatively you can build and install the Ruby extension from source as follows:

    $ rake
    $ sudo rake install

Usage
-----

Simply run Interactive Ruby:

    $ irb

You can load and use FFMpeg as shown below. The example is a simple video player.

    require 'rubygems'
    require 'hornetseye_ffmpeg'
    require 'hornetseye_xorg'
    require 'hornetseye_alsa'
    include Hornetseye
    raise 'Syntax: ./play.rb <video file> [offset]' unless ( 1 .. 2 ).member? ARGV.size
    input = AVInput.new ARGV.first
    input.pos = ARGV.last.to_i if ARGV.size > 1
    display = X11Display.new
    output = XVideoOutput.new
    w, h = ( input.width * input.aspect_ratio ).to_i, input.height
    window = X11Window.new display, output, w, h
    window.title = ARGV.first
    if input.has_audio?
      alsa = AlsaOutput.new 'default:0', input.sample_rate, input.channels, 16, 1024
      audio_frame = input.read_audio
    else
      t0 = Time.new.to_f
    end
    window.show
    while display.status?
      video_frame = input.read_video
      if input.has_audio?
        while alsa.avail >= audio_frame.shape[1]
          alsa.write audio_frame
          audio_frame = input.read_audio
        end
        t = input.audio_pos - alsa.delay.quo( alsa.rate )
      else
        t = Time.new.to_f - t0
      end
      delay = [ input.video_pos - t, 0 ].max
      display.event_loop delay
      output.write video_frame
      display.process_events
    end

