/* HornetsEye - Computer Vision with Ruby
   Copyright (C) 2006, 2007, 2008, 2009, 2010   Jan Wedekind

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */
#ifndef FRAME_HH
#define FRAME_HH

#include <boost/smart_ptr.hpp>
#include <ruby.h>

class Frame
{
public:
  Frame( const char *typecode, int width, int height, char *data );
  virtual ~Frame(void) {}
  VALUE rubyObject(void) { return m_frame; }
protected:
  VALUE m_frame;
};

typedef boost::shared_ptr< Frame > FramePtr;

#endif
