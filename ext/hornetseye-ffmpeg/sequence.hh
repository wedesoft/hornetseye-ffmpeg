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
#ifndef SEQUENCE_HH
#define SEQUENCE_HH

#include <boost/smart_ptr.hpp>
#include "rubyinc.hh"
#include <string>

class Sequence
{
public:
  Sequence( int size );
  Sequence( VALUE rbSequence ): m_sequence( rbSequence ) {}
  virtual ~Sequence(void) {}
  int size(void);
  char *data(void);
  VALUE rubyObject(void) { return m_sequence; }
  void markRubyMember(void);
protected:
  VALUE m_sequence;
};

typedef boost::shared_ptr< Sequence > SequencePtr;

#endif
