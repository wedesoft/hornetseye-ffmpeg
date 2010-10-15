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
#ifndef AVINPUT_HH
#define AVINPUT_HH

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boost/shared_ptr.hpp>
extern "C" {
#ifdef HAVE_LIBSWSCALE_INCDIR
  #include <libswscale/swscale.h>
#else
  #include <ffmpeg/swscale.h>
#endif
#ifdef HAVE_LIBAVFORMAT_INCDIR
  #include <libavformat/avformat.h>
#else
  #include <ffmpeg/avformat.h>
#endif
}
#include "rubyinc.hh"
#include "error.hh"
#include "frame.hh"

class AVInput
{
public:
  AVInput( const std::string &mrl ) throw (Error);
  virtual ~AVInput(void);
  void close(void);
  FramePtr read(void) throw (Error);
  bool status(void) const;
  int width(void) const throw (Error);
  int height(void) const throw (Error);
  AVRational timeBase(void) throw (Error);
  AVRational frameRate(void) throw (Error);
  long long duration(void) throw (Error);
  long long startTime(void) throw (Error);
  void seek( long long timestamp ) throw (Error);
  long long pts(void) throw (Error);
  static VALUE cRubyClass;
  static VALUE registerRubyClass( VALUE rbModule );
  static void deleteRubyObject( void *ptr );
  static VALUE wrapNew( VALUE rbClass, VALUE rbMRL );
  static VALUE wrapClose( VALUE rbSelf );
  static VALUE wrapRead( VALUE rbSelf );
  static VALUE wrapStatus( VALUE rbSelf );
  static VALUE wrapTimeBase( VALUE rbSelf );
  static VALUE wrapFrameRate( VALUE rbSelf );
  static VALUE wrapDuration( VALUE rbSelf );
  static VALUE wrapStartTime( VALUE rbSelf );
  static VALUE wrapWidth( VALUE rbSelf );
  static VALUE wrapHeight( VALUE rbSelf );
  static VALUE wrapSeek( VALUE rbSelf, VALUE rbPos );
  static VALUE wrapPTS( VALUE rbSelf );
protected:
  std::string m_mrl;
  AVFormatContext *m_ic;
  AVCodecContext *m_dec;
  AVCodec *m_codec;
  int m_idx;
  long long m_pts;
  struct SwsContext *m_swsContext;
  AVFrame *m_frame;
  boost::shared_array< char > m_data;
};

typedef boost::shared_ptr< AVInput > AVInputPtr;

#endif
