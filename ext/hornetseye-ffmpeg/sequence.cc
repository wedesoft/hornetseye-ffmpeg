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
#include "sequence.hh"

using namespace std;

Sequence::Sequence( int size ):
  m_sequence( Qnil )
{
  VALUE mModule = rb_define_module( "Hornetseye" );
  VALUE cMalloc = rb_define_class_under( mModule, "Malloc", rb_cObject );
  VALUE cSequence = rb_define_class_under( mModule, "Sequence", rb_cObject );
  VALUE rbSize = INT2NUM( size );
  VALUE rbMemory = rb_funcall( cMalloc, rb_intern( "new" ), 1, rbSize );
  m_sequence = rb_funcall( cSequence, rb_intern( "import" ), 3,
                           rb_const_get( mModule, rb_intern( "UBYTE" ) ),
                           rbMemory, rbSize );
}

int Sequence::size(void)
{
  return NUM2INT( rb_funcall( m_sequence, rb_intern( "size" ), 0 ) );
}

char *Sequence::data(void)
{
  VALUE rbMemory = rb_funcall( m_sequence, rb_intern( "memory" ), 0 );
  char *ptr;
  Data_Get_Struct( rbMemory, char, ptr );
  return ptr;
}

void Sequence::markRubyMember(void)
{
  rb_gc_mark( m_sequence );
}

