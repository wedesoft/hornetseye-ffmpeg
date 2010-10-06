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
#include <iostream>
#include "avinput.hh"

#if !defined(INT64_C)
#define INT64_C(c) c ## LL
#endif

// Also see FFMPEG tutorial at http://dranger.com/ffmpeg/

using namespace std;

VALUE AVInput::cRubyClass = Qnil;

AVInput::AVInput( const string &mrl ) throw (Error):
  m_mrl( mrl ), m_ic( NULL ), m_dec( NULL ), m_codec( NULL ), m_idx( -1 ),
  m_pts( 0 ), m_swsContext( NULL ), m_frame( NULL )
{
  try {
    int err = av_open_input_file( &m_ic, mrl.c_str(), NULL, 0, NULL );
    ERRORMACRO( err >= 0, Error, , "Error opening file \"" << mrl << "\": "
                << strerror( -err ) );
    err = av_find_stream_info( m_ic );
    ERRORMACRO( err >= 0, Error, , "Error finding stream info for file \""
                << mrl << "\": " << strerror( -err ) );
    for ( int i=0; i<m_ic->nb_streams; i++ )
      if ( m_ic->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO ) {
        m_idx = i;
        break;
      };
    ERRORMACRO( m_idx >= 0, Error, , "Could not find video stream in file \""
                << mrl << "\"" );
    m_dec = m_ic->streams[ m_idx ]->codec;
    m_codec = avcodec_find_decoder( m_dec->codec_id );
    ERRORMACRO( m_codec != NULL, Error, , "Could not find decoder for file \""
                << mrl << "\"" );
    err = avcodec_open( m_dec, m_codec );
    ERRORMACRO( err >= 0, Error, , "Error opening codec for file \""
                << mrl << "\": " << strerror( -err ) );
    m_swsContext = sws_getContext( m_dec->width, m_dec->height, m_dec->pix_fmt,
                                   m_dec->width, m_dec->height, PIX_FMT_YUV420P,
                                   SWS_FAST_BILINEAR, 0, 0, 0 );
    m_frame = avcodec_alloc_frame();
    ERRORMACRO( m_frame, Error, , "Error allocating frame" );
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
  if ( m_frame ) {
    av_free( m_frame );
    m_frame = NULL;
  };
  if ( m_swsContext ) {
    sws_freeContext( m_swsContext );
    m_swsContext = NULL;
  };
  if ( m_codec ) {
    m_ic->streams[ m_idx ]->discard = AVDISCARD_ALL;
    avcodec_close( m_dec );
    m_codec = NULL;
  };
  m_dec = NULL;
  m_idx = -1;
  if ( m_ic ) {
    av_close_input_file( m_ic );
    m_ic = NULL;
  };
}

FramePtr AVInput::read(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  FramePtr retVal;
  AVPacket packet;
  while ( av_read_frame( m_ic, &packet ) >= 0 ) {
    if ( packet.stream_index == m_idx ) {
      int frameFinished;
      int err = avcodec_decode_video( m_dec, m_frame, &frameFinished,
                                      packet.data, packet.size );
      ERRORMACRO( err >= 0, Error, ,
                  "Error decoding frame of video \"" << m_mrl << "\"" );

      if ( frameFinished ) {
        if ( packet.dts != AV_NOPTS_VALUE ) m_pts = packet.dts;
        av_free_packet( &packet );
        AVFrame picture;
        m_data = boost::shared_array< char >( new char[ m_dec->width *
                                                        m_dec->height *
                                                        3 / 2 ] );
        picture.data[0] = (uint8_t *)m_data.get();
        picture.data[1] = (uint8_t *)m_data.get() +
                          m_dec->width * m_dec->height * 5 / 4;
        picture.data[2] = (uint8_t *)m_data.get() + m_dec->width * m_dec->height;
        picture.linesize[0] = m_dec->width;
        picture.linesize[1] = m_dec->width / 2;
        picture.linesize[2] = m_dec->width / 2;
        sws_scale( m_swsContext, m_frame->data, m_frame->linesize, 0,
                   m_dec->height, picture.data, picture.linesize );
        retVal = FramePtr( new Frame( "YV12", m_dec->width, m_dec->height,
                                      m_data.get() ) );
        break;
      };
    };
    av_free_packet( &packet );
  };
  ERRORMACRO( retVal.get(), Error, , "No more frames available" );
  return retVal;
}

int AVInput::width(void) const throw (Error)
{
  ERRORMACRO( m_dec != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_dec->width;
}

int AVInput::height(void) const throw (Error)
{
  ERRORMACRO( m_dec != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_dec->height;
}

AVRational AVInput::timeBase(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_idx ]->time_base;
}

AVRational AVInput::frameRate(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_idx ]->r_frame_rate;
}

long long AVInput::duration(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_idx ]->duration;
}

long long AVInput::startTime(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_ic->streams[ m_idx ]->start_time;
}

void AVInput::seek( long timestamp ) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  ERRORMACRO( av_seek_frame( m_ic, -1, timestamp, 0 ) >= 0,
              Error, , "Error seeking in video \"" << m_mrl << "\"" );
  avcodec_flush_buffers( m_dec );
}

long long AVInput::pts(void) throw (Error)
{
  ERRORMACRO( m_ic != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_pts;
}

VALUE AVInput::registerRubyClass( VALUE rbModule )
{
  cRubyClass = rb_define_class_under( rbModule, "AVInput", rb_cObject );
  rb_define_singleton_method( cRubyClass, "new",
                              RUBY_METHOD_FUNC( wrapNew ), 1 );
  rb_define_const( cRubyClass, "AV_TIME_BASE", INT2NUM( AV_TIME_BASE ) );
  rb_define_method( cRubyClass, "close", RUBY_METHOD_FUNC( wrapClose ), 0 );
  rb_define_method( cRubyClass, "read", RUBY_METHOD_FUNC( wrapRead ), 0 );
  rb_define_method( cRubyClass, "time_base", RUBY_METHOD_FUNC( wrapTimeBase ), 0 );
  rb_define_method( cRubyClass, "frame_rate", RUBY_METHOD_FUNC( wrapFrameRate ), 0 );
  rb_define_method( cRubyClass, "duration", RUBY_METHOD_FUNC( wrapDuration ), 0 );
  rb_define_method( cRubyClass, "start_time", RUBY_METHOD_FUNC( wrapStartTime ), 0 );
  rb_define_method( cRubyClass, "width", RUBY_METHOD_FUNC( wrapWidth ), 0 );
  rb_define_method( cRubyClass, "height", RUBY_METHOD_FUNC( wrapHeight ), 0 );
  rb_define_method( cRubyClass, "seek", RUBY_METHOD_FUNC( wrapSeek ), 1 );
  rb_define_method( cRubyClass, "pts", RUBY_METHOD_FUNC( wrapPTS ), 0 );
}

void AVInput::deleteRubyObject( void *ptr )
{
  delete (AVInputPtr *)ptr;
}

VALUE AVInput::wrapNew( VALUE rbClass, VALUE rbMRL )
{
  VALUE retVal = Qnil;
  try {
    rb_check_type( rbMRL, T_STRING );
    AVInputPtr ptr( new AVInput( StringValuePtr( rbMRL ) ) );
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

VALUE AVInput::wrapRead( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    FramePtr frame( (*self)->read() );
    retVal = frame->rubyObject();
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapTimeBase( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    AVRational timeBase = (*self)->timeBase();
    retVal = rb_funcall( rb_cObject, rb_intern( "Rational" ), 2,
                         INT2NUM( timeBase.num ), INT2NUM( timeBase.den ) );
  } catch( exception &e ) {
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
  } catch( exception &e ) {
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
  } catch( exception &e ) {
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
  } catch( exception &e ) {
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
  } catch( exception &e ) {
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
  } catch( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapSeek( VALUE rbSelf, VALUE rbPos )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    (*self)->seek( NUM2LL( rbPos ) );
  } catch( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVInput::wrapPTS( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVInputPtr *self; Data_Get_Struct( rbSelf, AVInputPtr, self );
    retVal = LL2NUM( (*self)->pts() );
  } catch( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

