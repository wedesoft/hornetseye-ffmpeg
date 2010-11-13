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

      def new( mrl, video_bit_rate, width, height, frame_rate, video_codec = nil,
               have_audio = false, audio_bit_rate = 64000, sample_rate = 44100,
               channels = 2, audio_codec = nil )
        if frame_rate.is_a? Float
          frame_rate = Rational( 90000, ( 90000 / frame_rate ).to_i )
        end
        orig_new mrl, video_bit_rate, width, height,
                 frame_rate.denominator, frame_rate.numerator,
                 video_codec || CODEC_ID_NONE,
                 have_audio ? audio_bit_rate : 0,
                 have_audio ? sample_rate : 0,
                 have_audio ? channels : 0,
                 audio_codec || CODEC_ID_NONE
      end

    end

    alias_method :write, :write_video

    alias_method :orig_write_video, :write_video

    def write_video( frame )
      orig_write_video frame.to_yv12
    end

    alias_method :orig_write_audio, :write_audio

    def write_audio( frame )
      if frame.shape != [ channels, frame_size ]
        raise "Audio frame must have #{channels} channels and #{frame_size} " +
              "samples (but had #{frame.shape[0]} channels and #{frame.shape[1]} " +
              "samples)"
      end
      orig_write_audio Sequence( UBYTE, frame.storage_size ).new( frame.memory )
    end

  end

end

