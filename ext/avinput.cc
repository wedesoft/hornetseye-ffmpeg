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
#include "avinput.hh"

using namespace std;

VALUE AVInput::cRubyClass = Qnil;

AVInput::AVInput( const string &mrl ) throw (Error):
  m_mrl( mrl ), m_ic( NULL ), m_enc( NULL ), m_codec( NULL ), m_idx( -1 ),
  m_frame( NULL )
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
    m_enc = m_ic->streams[ m_idx ]->codec;
    m_codec = avcodec_find_decoder( m_enc->codec_id );
    ERRORMACRO( m_codec != NULL, Error, , "Could not find decoder for file \""
                << mrl << "\"" );
    err = avcodec_open( m_enc, m_codec );
    ERRORMACRO( err >= 0, Error, , "Error opening codec for file \""
                << mrl << "\": " << strerror( -err ) );
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
  if ( m_codec ) {
    avcodec_close( m_enc );
    m_codec = NULL;
  };
  m_enc = NULL;
  m_idx = -1;
  if ( m_ic ) {
    av_close_input_file( m_ic );
    m_ic = NULL;
  };
}

FramePtr AVInput::read(void) throw (Error)
{
  FramePtr retVal;
  AVPacket packet;
  while ( av_read_frame( m_ic, &packet ) >= 0 ) {
    if ( packet.stream_index == m_idx ) {
      int frameFinished;
      int err = avcodec_decode_video( m_enc, m_frame, &frameFinished,
                                      packet.data, packet.size );
      ERRORMACRO( err >= 0, Error, ,
                  "Error decoding frame of video \"" << m_mrl << "\"" );
      if ( frameFinished ) {
        av_free_packet( &packet );
        AVFrame frame;
        m_data = boost::shared_array< char >( new char[ m_enc->width *
                                                        m_enc->height *
                                                        3 / 2 ] );
        frame.data[0] = (uint8_t *)m_data.get();
        frame.data[1] = (uint8_t *)m_data.get() +
                        m_enc->width * m_enc->height * 5 / 4;
        frame.data[2] = (uint8_t *)m_data.get() + m_enc->width * m_enc->height;
        frame.linesize[0] = m_enc->width;
        frame.linesize[1] = m_enc->width / 2;
        frame.linesize[2] = m_enc->width / 2;
        struct SwsContext *swsContext =
          sws_getContext( m_enc->width, m_enc->height, m_enc->pix_fmt,
                          m_enc->width, m_enc->height, PIX_FMT_YUV420P,
                          SWS_BILINEAR, 0, 0, 0 );
        sws_scale( swsContext, m_frame->data, m_frame->linesize, 0,
                   m_enc->height, frame.data, frame.linesize );
        sws_freeContext( swsContext );
        retVal = FramePtr( new Frame( "YV12", m_enc->width, m_enc->height,
                                      m_data.get() ) );
        break;
      };
    };
    av_free_packet( &packet );
  };
  ERRORMACRO( retVal.get(), Error, , "No more frames available" );
  return retVal;
}

VALUE AVInput::registerRubyClass( VALUE rbModule )
{
  av_register_all();
  cRubyClass = rb_define_class_under( rbModule, "AVInput", rb_cObject );
  rb_define_singleton_method( cRubyClass, "new",
                              RUBY_METHOD_FUNC( wrapNew ), 1 );
  rb_define_method( cRubyClass, "read", RUBY_METHOD_FUNC( wrapRead ), 0 );
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
