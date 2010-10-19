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

  class AVInput

    class << self

      alias_method :orig_new, :new

      def new( mrl )
        retval = orig_new mrl
        retval.instance_eval do
          @frame = nil
          @video = Queue.new
          @audio = Queue.new
          @video_pts = AV_NOPTS_VALUE
          @audio_pts = AV_NOPTS_VALUE
        end
        retval
      end

    end

    def shape
      [ width, height ]
    end

    def read_video
      enqueue_frame while @video.empty?
      frame, @video_pts = @video.deq
      frame
    end

    alias_method :read, :read_video

    def read_audio
      enqueue_frame while @audio.empty?
      frame, @audio_pts = @audio.deq
      frame
    end

    def enqueue_frame
      frame = read_av
      if frame.is_a? Frame_
        @video.enq [ frame, video_pts ]
        @frame = frame
      else
        n = channels
        target = Hornetseye::MultiArray SINT, n, frame.size / ( 2 * n )
        @audio.enq [ target.new( frame.memory ), audio_pts ]
      end
    end

    def pos=( timestamp )
      unless @frame
        begin
          read_video
        rescue Exception
        end
      end
      seek timestamp * AV_TIME_BASE
    end

    def video_pos
      @video_pts == AV_NOPTS_VALUE ? nil : @video_pts * video_time_base
    end

    alias_method :pos, :video_pos

    def audio_pos
      @audio_pts == AV_NOPTS_VALUE ? nil : @audio_pts * audio_time_base
    end

    alias_method :orig_duration, :duration

    def duration
      orig_duration == AV_NOPTS_VALUE ? nil : orig_duration * video_time_base
    end

    alias_method :orig_start_time, :start_time

    def video_start_time
      orig_start_time == AV_NOPTS_VALUE ? nil : orig_start_time * video_time_base
    end

  end

end

