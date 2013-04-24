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
#ifndef AVOUTPUT_HH
#define AVOUTPUT_HH

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
#include "sequence.hh"

class AVOutput
{
public:
  AVOutput( const std::string &mrl, int videoBitRate, int width, int height,
            int timeBaseNum, int timeBaseDen, int aspectRatioNum,
            int aspectRatioDen, enum AVCodecID videoCodecID,
            int audioBitRate, int sampleRate, int channels,
            enum AVCodecID audioCodecID) throw (Error);
  virtual ~AVOutput(void);
  void close(void);
  AVRational videoTimeBase(void) throw (Error);
  AVRational audioTimeBase(void) throw (Error);
  int frameSize(void) throw (Error);
  int channels(void) throw (Error);
  void writeVideo( FramePtr frame ) throw (Error);
  void writeAudio( SequencePtr frame ) throw (Error);
  static VALUE cRubyClass;
  static VALUE registerRubyClass( VALUE rbModule );
  static void deleteRubyObject( void *ptr );
  static VALUE wrapNew( VALUE rbClass, VALUE rbMRL, VALUE rbBitRate, VALUE rbWidth,
                        VALUE rbHeight, VALUE rbTimeBaseNum, VALUE rbTimeBaseDen,
                        VALUE rbAspectRatioNum, VALUE rbAspectRatioDen,
                        VALUE rbVideoCodecID, VALUE rbAudioBitRate, VALUE rbSampleRate,
                        VALUE rbChannels, VALUE rbAudioCodecID);
  static VALUE wrapClose( VALUE rbSelf );
  static VALUE wrapVideoTimeBase( VALUE rbSelf );
  static VALUE wrapAudioTimeBase( VALUE rbSelf );
  static VALUE wrapFrameSize( VALUE rbSelf );
  static VALUE wrapChannels( VALUE rbSelf );
  static VALUE wrapWriteVideo( VALUE rbSelf, VALUE rbFrame );
  static VALUE wrapWriteAudio( VALUE rbSelf, VALUE rbFrame );
protected:
  std::string m_mrl;
  AVFormatContext *m_oc;
  AVStream *m_videoStream;
  AVStream *m_audioStream;
  bool m_videoCodecOpen;
  bool m_audioCodecOpen;
  char *m_videoBuf;
  char *m_audioBuf;
  bool m_fileOpen;
  bool m_headerWritten;
  struct SwsContext *m_swsContext;
  AVFrame *m_frame;
};

typedef boost::shared_ptr< AVOutput > AVOutputPtr;

#endif

