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
        retval.instance_eval { @frame = nil }
        retval
      end

    end

    def shape
      [ width, height ]
    end

    alias_method :orig_read, :read

    def read
      @frame = orig_read
    end

    def pos=( timestamp )
      unless @frame
        begin
          read
        rescue Exception
        end
      end
      seek timestamp * AV_TIME_BASE
    end

    def pos
      pts * time_base
    end

    alias_method :orig_duration, :duration

    def duration
      orig_duration * time_base
    end

  end

end

