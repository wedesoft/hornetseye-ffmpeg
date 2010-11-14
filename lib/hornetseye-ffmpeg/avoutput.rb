# hornetseye-ffmpeg - Read/write video frames using libffmpeg
# Copyright (C) 2010 Jan Wedekind
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Namespace of Hornetseye computer vision library
module Hornetseye

  class AVOutput

    class << self

      alias_method :orig_new, :new

      def new( mrl, video_bit_rate, width, height, frame_rate, aspect_ratio = 1,
               video_codec = nil, have_audio = false, audio_bit_rate = 64000,
               sample_rate = 44100, channels = 2, audio_codec = nil )
        if frame_rate.is_a? Float
          frame_rate = Rational( 90000, ( 90000 / frame_rate ).to_i )
        end
        retval = orig_new mrl, video_bit_rate, width, height,
                          frame_rate.denominator, frame_rate.numerator,
                          aspect_ratio.numerator, aspect_ratio.denominator,
                          video_codec || CODEC_ID_NONE,
                          have_audio ? audio_bit_rate : 0,
                          have_audio ? sample_rate : 0,
                          have_audio ? channels : 0,
                          audio_codec || CODEC_ID_NONE
        if have_audio
          retval.instance_eval do
            @audio_buffer = Hornetseye::MultiArray( SINT, channels, frame_size ).new
            @audio_samples = 0
          end
        end
        retval
      end

    end

    alias_method :orig_write_video, :write_video

    def write_video( frame )
      orig_write_video frame.to_yv12
    end

    alias_method :write, :write_video

    alias_method :orig_write_audio, :write_audio

    def write_audio( frame )
      unless frame.typecode == SINT
        raise "Audio frame must have elements of type SINT (but elements were of " +
              "type #{frame.typecode})"
      end
      unless frame.dimension == 2
        raise "Audio frame must have 2 dimensions (but had #{frame.dimensions})"
      end
      unless frame.shape.first == channels
        raise "Audio frame must have #{channels} channels " +
              "(but had #{frame.shape.first})"
      end
      frame_type = Hornetseye::Sequence UBYTE, frame_size * 2 * channels
      remaining = frame
      while @audio_samples + remaining.shape.last >= frame_size
        if @audio_samples > 0
          @audio_buffer[ @audio_samples ... frame_size ] =
            remaining[ 0 ... frame_size - @audio_samples ]
          orig_write_audio frame_type.new( @audio_buffer.memory )
          remaining = remaining[ 0 ... channels,
                                 frame_size - @audio_samples ... remaining.shape.last ]
          @audio_samples = 0
        else
          orig_write_audio frame_type.new( remaining.memory )
          remaining = remaining[ 0 ... channels, frame_size ... remaining.shape.last ]
        end
      end
      if remaining.shape.last > 0
        @audio_buffer[ 0 ... channels, @audio_samples ... @audio_samples +
                       remaining.shape.last ] = remaining
        @audio_samples += remaining.shape.last
      end
      frame
    end

  end

end

