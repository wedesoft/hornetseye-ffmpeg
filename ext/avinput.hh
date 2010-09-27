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
#ifndef AVFORMAT_HH
#define AVFORMAT_HH

#include <boost/shared_ptr.hpp>
#include <ruby.h>
extern "C" {
  #include <libswscale/swscale.h>
  #include <libavformat/avformat.h>
}
#include "error.hh"
#include "frame.hh"

class AVInput
{
public:
  AVInput( const std::string &mrl ) throw (Error);
  virtual ~AVInput(void);
  void close(void);
  FramePtr read(void) throw (Error);
  AVRational timeBase(void) throw (Error);
  void seek( long timestamp ) throw (Error);
  long tell(void) throw (Error);
  static VALUE cRubyClass;
  static VALUE registerRubyClass( VALUE rbModule );
  static void deleteRubyObject( void *ptr );
  static VALUE wrapNew( VALUE rbClass, VALUE rbMRL );
  static VALUE wrapRead( VALUE rbSelf );
  static VALUE wrapTimeBase( VALUE rbSelf );
  static VALUE wrapSeek( VALUE rbSelf, VALUE rbPos );
  static VALUE wrapTell( VALUE rbSelf );
protected:
  std::string m_mrl;
  AVFormatContext *m_ic;
  AVCodecContext *m_enc;
  AVCodec *m_codec;
  int m_idx;
  AVFrame *m_frame;
  boost::shared_array< char > m_data;
};

typedef boost::shared_ptr< AVInput > AVInputPtr;

#endif
