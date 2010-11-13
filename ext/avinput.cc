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
#ifndef NDEBUG
#include <iostream>
#endif
#include <malloc.h>
#include "avinput.hh"

#if !defined(INT64_C)
#define INT64_C(c) c ## LL
#endif

// Also see FFMPEG tutorial at http://dranger.com/ffmpeg/

using namespace std;

VALUE AVInput::cRubyClass = Qnil;

AVInput::AVInput( const string &mrl, bool audio ) throw (Error):
  m_mrl( mrl ), m_ic( NULL ), m_videoDec( NULL ), m_audioDec( NULL ),
  m_videoCodec( NULL ), m_audioCodec( NULL ),
  m_videoStream( -1 ), m_audioStream( -1 ), m_videoPts( 0 ), m_audioPts( 0 ),
  m_swsContext( NULL ), m_avFrame( NULL )
{
  try {
    int err = av_open_input_file( &m_ic, mrl.c_str(), NULL, 0, NULL );
    ERRORMACRO( err >= 0, Error, , "Error opening file \"" << mrl << "\": "
                << strerror( errno ) );
    err = av_find_stream_info( m_ic );
    ERRORMACRO( err >= 0, Error, , "Error finding stream info for file \""
                << mrl << "\": " << strerror( errno ) );
    for ( unsigned int i=0; i<m_ic->nb_streams; i++ ) {
      if ( m_ic->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO )
        m_videoStream = i;
      if ( audio && m_ic->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO )
          m_audioStream = i;
    };
#ifndef NDEBUG
    cerr << "Video stream index is " << m_videoStream << endl;
    cerr << "Audio stream index is " << m_audioStream << endl;
#endif
    ERRORMACRO( m_videoStream >= 0, Error, ,
                "Could not find video stream in file \"" << mrl << "\"" );
    m_videoDec = m_ic->streams[ m_videoStream ]->codec;
    if ( m_audioStream >= 0 )
      m_audioDec = m_ic->streams[ m_audioStream ]->codec;
    m_videoCodec = avcodec_find_decoder( m_videoDec->codec_id );
    ERRORMACRO( m_videoCodec != NULL, Error, , "Could not find video decoder for "
                "file \"" << mrl << "\"" );
    err = avcodec_open( m_videoDec, m_videoCodec );
    if ( err < 0 ) {
      m_videoCodec = NULL;
      ERRORMACRO( false, Error, , "Error opening video codec for file \""
                  << mrl << "\": " << strerror( errno ) );
    };
    if ( m_audioDec != NULL ) {
      m_audioCodec = avcodec_find_decoder( m_audioDec->codec_id );
      ERRORMACRO( m_audioCodec != NULL, Error, , "Could not find audio decoder for "
                  "file \"" << mrl << "\"" );
      err = avcodec_open( m_audioDec, m_audioCodec );
      if ( err < 0 ) {
        m_audioCodec = NULL;
        ERRORMACRO( false, Error, , "Error opening audio codec for file \""
                    << mrl << "\": " << strerror( errno ) );
      };
    };
    m_swsContext = sws_getContext( m_videoDec->width, m_videoDec->height,
                                   m_videoDec->pix_fmt,
                                   m_videoDec->width, m_videoDec->height,
                                   PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0 );
    m_avFrame = avcodec_alloc_frame();
    ERRORMACRO( m_avFrame, Error, , "Error allocating frame" );
  } catch ( Error &e ) {
    close();
    throw e;
  };
}

AVInput::~AVInput(void)
{
  close();
}

void AVInput::close(void)
{
  m_audioFrame.reset();
  m_videoFrame.reset();
  if ( m_avFrame ) {
    av_free( m_avFrame );
    m_avFrame = NULL;
  };
  if ( m_swsContext ) {
    sws_freeContext( m_swsContext );
    m_swsContext = NULL;
  };
  if ( m_audioCodec ) {
    avcodec_close( m_audioDec );
    m_audioCodec = NULL;
  };
  if ( m_videoCodec ) {
    m_ic->streams[ m_videoStream ]->discard = AVDISCARD_ALL;
    avcodec_close( m_videoDec );
    m_videoCodec = NULL;
  };
  m_audioDec = NULL;
  m_videoDec = NULL;
  m_audioStream = -1;
  m_videoStream = -1;
  if ( m_ic ) {
    av_close_input_file( m_ic );
    m_ic = NULL;
  };
}

void AVInput::readAV(void) throw (Error)
{
  m_audioFrame.reset();
  m_videoFrame.reset();
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  AVPacket packet;
  long long firstPacketPts = AV_NOPTS_VALUE;
  while ( av_read_frame( m_ic, &packet ) >= 0 ) {
    if ( packet.stream_index == m_videoStream ) {
      int frameFinished;
      int err = avcodec_decode_video( m_videoDec, m_avFrame, &frameFinished,
                                      packet.data, packet.size );
      ERRORMACRO( err >= 0, Error, ,
                  "Error decoding video frame of video \"" << m_mrl << "\"" );
      if ( firstPacketPts == AV_NOPTS_VALUE ) firstPacketPts = packet.pts;
      if ( frameFinished ) {
        if ( packet.dts != AV_NOPTS_VALUE )
          m_videoPts = packet.dts;
        else
          m_videoPts = firstPacketPts;
        av_free_packet( &packet );
        AVFrame picture;
        m_videoFrame = FramePtr( new Frame( "YV12", m_videoDec->width,
                                            m_videoDec->height ) );
        int
          width   = m_videoDec->width,
          height  = m_videoDec->height,
          width2  = ( width  + 1 ) / 2,
          height2 = ( height + 1 ) / 2,
          widtha  = ( width  + 7 ) & ~0x7,
          width2a = ( width2 + 7 ) & ~0x7;
        picture.data[0] = (uint8_t *)m_videoFrame->data();
        picture.data[2] = (uint8_t *)m_videoFrame->data() + widtha * height;
        picture.data[1] = (uint8_t *)picture.data[2] + width2a * height2;
        picture.linesize[0] = widtha;
        picture.linesize[1] = width2a;
        picture.linesize[2] = width2a;
        sws_scale( m_swsContext, m_avFrame->data, m_avFrame->linesize, 0,
                   m_videoDec->height, picture.data, picture.linesize );
        break;
      } else
        av_free_packet( &packet );
    } else if ( packet.stream_index == m_audioStream ) {
      if ( packet.pts != AV_NOPTS_VALUE ) m_audioPts = packet.pts;
      m_audioFrame.reset();
      unsigned char *data = packet.data;
      int size = packet.size;
      while ( size > 0 ) {
        short int *buffer =
          (short int *)memalign( 16, AVCODEC_MAX_AUDIO_FRAME_SIZE * 3 / 2 );
        buffer[ AVCODEC_MAX_AUDIO_FRAME_SIZE * 3 / 4 - 1 ] = '\000';
        int bufSize = AVCODEC_MAX_AUDIO_FRAME_SIZE * 3 / 2;
        int len = avcodec_decode_audio2( m_audioDec, buffer, &bufSize, data, size );
        if ( len < 0 ) {
          free( buffer );
          m_audioFrame.reset();
          ERRORMACRO( false, Error, , "Error decoding audio frame of video \""
                      << m_mrl << "\"" );
        };
        data += len;
        size -= len;
        if ( bufSize > 0 ) {
          if ( m_audioFrame.get() ) {
            SequencePtr extended( new Sequence( m_audioFrame->size() + bufSize ) );
            memcpy( extended->data(), m_audioFrame->data(), m_audioFrame->size() );
            memcpy( extended->data() + m_audioFrame->size(), buffer, bufSize );
            m_audioFrame = extended;
          } else {
            m_audioFrame = SequencePtr( new Sequence( bufSize ) );
            memcpy( m_audioFrame->data(), buffer, bufSize );
          };
        };
        free( buffer );
      };
      av_free_packet( &packet );
      if ( m_audioFrame.get() ) break;
    } else
      av_free_packet( &packet );
  };
  ERRORMACRO( m_videoFrame.get() || m_audioFrame.get(), Error, ,
              "No more frames available" );
}

bool AVInput::status(void) const
{
  return m_ic != NULL;
}

int AVInput::width(void) const throw (Error)
{
  ERRORMACRO( m_videoDec != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_videoDec->width;
}

int AVInput::height(void) const throw (Error)
{
  ERRORMACRO( m_videoDec != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_videoDec->height;
}

bool AVInput::hasAudio(void) const
{
  return m_audioStream != -1;
}

AVRational AVInput::videoTimeBase(void) throw (Error)
{
  ERRORMACRO( m_videoStream != -1, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_videoStream ]->time_base;
}

AVRational AVInput::audioTimeBase(void) throw (Error)
{
  ERRORMACRO( m_audioStream != -1, Error, , "Audio \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_audioStream ]->time_base;
}

AVRational AVInput::frameRate(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_videoStream ]->r_frame_rate;
}

AVRational AVInput::aspectRatio(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_videoStream ]->sample_aspect_ratio;
}

int AVInput::sampleRate(void) throw (Error)
{
  ERRORMACRO( m_audioDec != NULL, Error, , "Audio \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_audioDec->sample_rate;
}

int AVInput::channels(void) throw (Error)
{
  ERRORMACRO( m_audioDec != NULL, Error, , "Audio \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_audioDec->channels;
}

long long AVInput::duration(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_videoStream ]->duration;
}

long long AVInput::startTime(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_videoStream ]->start_time;
}

void AVInput::seek( long long timestamp ) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  ERRORMACRO( av_seek_frame( m_ic, -1, timestamp, 0 ) >= 0,
              Error, , "Error seeking in video \"" << m_mrl << "\"" );
  avcodec_flush_buffers( m_videoDec );
}

long long AVInput::videoPts(void) throw (Error)
{
  ERRORMACRO( m_videoStream != -1, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_videoPts;
}

long long AVInput::audioPts(void) throw (Error)
{
  ERRORMACRO( m_audioStream != -1, Error, , "Audio \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_audioPts;
}

VALUE AVInput::registerRubyClass( VALUE rbModule )
{
  cRubyClass = rb_define_class_under( rbModule, "AVInput", rb_cObject );
  rb_define_singleton_method( cRubyClass, "new",
                              RUBY_METHOD_FUNC( wrapNew ), 2 );
  rb_define_const( cRubyClass, "AV_TIME_BASE", INT2NUM( AV_TIME_BASE ) );
  rb_define_const( cRubyClass, "AV_NOPTS_VALUE", LL2NUM( AV_NOPTS_VALUE ) );
  rb_define_method( cRubyClass, "close", RUBY_METHOD_FUNC( wrapClose ), 0 );
  rb_define_method( cRubyClass, "read_av", RUBY_METHOD_FUNC( wrapReadAV ), 0 );
  rb_define_method( cRubyClass, "status?", RUBY_METHOD_FUNC( wrapStatus ), 0 );
  rb_define_method( cRubyClass, "video_time_base",
                    RUBY_METHOD_FUNC( wrapVideoTimeBase ), 0 );
  rb_define_method( cRubyClass, "audio_time_base",
                    RUBY_METHOD_FUNC( wrapAudioTimeBase ), 0 );
  rb_define_method( cRubyClass, "frame_rate", RUBY_METHOD_FUNC( wrapFrameRate ), 0 );
  rb_define_method( cRubyClass, "aspect_ratio",
                    RUBY_METHOD_FUNC( wrapAspectRatio ), 0 );
  rb_define_method( cRubyClass, "sample_rate", RUBY_METHOD_FUNC( wrapSampleRate ), 0 );
  rb_define_method( cRubyClass, "channels", RUBY_METHOD_FUNC( wrapChannels ), 0 );
  rb_define_method( cRubyClass, "duration", RUBY_METHOD_FUNC( wrapDuration ), 0 );
  rb_define_method( cRubyClass, "start_time", RUBY_METHOD_FUNC( wrapStartTime ), 0 );
  rb_define_method( cRubyClass, "width", RUBY_METHOD_FUNC( wrapWidth ), 0 );
  rb_define_method( cRubyClass, "height", RUBY_METHOD_FUNC( wrapHeight ), 0 );
  rb_define_method( cRubyClass, "has_audio?", RUBY_METHOD_FUNC( wrapHasAudio ), 0 );
  rb_define_method( cRubyClass, "seek", RUBY_METHOD_FUNC( wrapSeek ), 1 );
  rb_define_method( cRubyClass, "video_pts", RUBY_METHOD_FUNC( wrapVideoPTS ), 0 );
  rb_define_method( cRubyClass, "audio_pts", RUBY_METHOD_FUNC( wrapAudioPTS ), 0 );
}

void AVInput::deleteRubyObject( void *ptr )
{
  delete (AVInputPtr *)ptr;
}

VALUE AVInput::wrapNew( VALUE rbClass, VALUE rbMRL, VALUE rbAudio )
{
  VALUE retVal = Qnil;
  try {
    rb_check_type( rbMRL, T_STRING );
    AVInputPtr ptr( new AVInput( StringValuePtr( rbMRL ), rbAudio == Qtrue ) );
    retVal = Data_Wrap_Struct( rbClass, 0, deleteRubyObject,
                               new AVInputPtr( ptr ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapClose( VALUE rbSelf )
{
  AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
  (*self)->close();
  return rbSelf;
}

VALUE AVInput::wrapReadAV( VALUE rbSelf )
{
  AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
  return (*self)->wrapReadAVInst();
}

VALUE AVInput::wrapReadAVInst(void)
{
  VALUE retVal = Qnil;
  try {
    readAV();
    if ( m_videoFrame.get() )
      retVal = m_videoFrame->rubyObject();
    if ( m_audioFrame.get() )
      retVal = m_audioFrame->rubyObject();
    m_audioFrame.reset();
    m_videoFrame.reset();
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapStatus( VALUE rbSelf )
{
  AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
  return (*self)->status() ? Qtrue : Qfalse;
}

VALUE AVInput::wrapVideoTimeBase( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    AVRational videoTimeBase = (*self)->videoTimeBase();
    retVal = rb_funcall( rb_cObject, rb_intern( "Rational" ), 2,
                         INT2NUM( videoTimeBase.num ), INT2NUM( videoTimeBase.den ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapAudioTimeBase( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    AVRational audioTimeBase = (*self)->audioTimeBase();
    retVal = rb_funcall( rb_cObject, rb_intern( "Rational" ), 2,
                         INT2NUM( audioTimeBase.num ), INT2NUM( audioTimeBase.den ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapFrameRate( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    AVRational frameRate = (*self)->frameRate();
    retVal = rb_funcall( rb_cObject, rb_intern( "Rational" ), 2,
                         INT2NUM( frameRate.num ), INT2NUM( frameRate.den ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapAspectRatio( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    AVRational aspectRatio = (*self)->aspectRatio();
    retVal = rb_funcall( rb_cObject, rb_intern( "Rational" ), 2,
                         INT2NUM( aspectRatio.num ), INT2NUM( aspectRatio.den ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapSampleRate( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = INT2NUM( (*self)->sampleRate() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapChannels( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = INT2NUM( (*self)->channels() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapDuration( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = LL2NUM( (*self)->duration() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapStartTime( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = LL2NUM( (*self)->startTime() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapWidth( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = INT2NUM( (*self)->width() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapHeight( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = INT2NUM( (*self)->height() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapHasAudio( VALUE rbSelf )
{
  AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
  return (*self)->hasAudio() ? Qtrue : Qfalse;
}

VALUE AVInput::wrapSeek( VALUE rbSelf, VALUE rbPos )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    (*self)->seek( NUM2LL( rbPos ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapVideoPTS( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = LL2NUM( (*self)->videoPts() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapAudioPTS( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = LL2NUM( (*self)->audioPts() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

