# hornetseye-frame - Colourspace conversions and compression
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
        retval.instance_eval { @initial = true }
        retval
      end

    end

    alias_method :orig_read, :read

    def read
      @initial = false
      orig_read
    end

    alias_method :orig_pos, :pos=

    def pos=( timestamp )
      if @initial
        begin
          read
        rescue Exception
        ensure
          @initial = false
        end
      end
      orig_pos timestamp
    end

  end

end

