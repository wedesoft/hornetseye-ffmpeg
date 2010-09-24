#include "avinput.hh"

using namespace std;

VALUE AVInput::cRubyClass = Qnil;

AVInput::AVInput( const string &mrl ) throw (Error):
  m_mrl( mrl ), m_ic( NULL ), m_enc( NULL ), m_codec( NULL ), m_idx( -1 ),
  m_frame( NULL )
{
  try {
    int err = av_open_input_file( &m_ic, m_mrl.c_str(), NULL, 0, NULL );
    ERRORMACRO( err >= 0, Error, , "Error opening file \"" << m_mrl << "\": "
                << strerror( -err ) );
    err = av_find_stream_info( m_ic );
    ERRORMACRO( err >= 0, Error, , "Error finding stream info for file \""
                << m_mrl << "\": " << strerror( -err ) );
    for ( int i=0; i<m_ic->nb_streams; i++ )
      if ( m_ic->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO ) {
        m_idx = i;
        break;
      };
    ERRORMACRO( m_idx >= 0, Error, , "Could not find video stream in file \""
                << m_mrl << "\"" );
    m_enc = m_ic->streams[ m_idx ]->codec;
    m_codec = avcodec_find_decoder( m_enc->codec_id );
    ERRORMACRO( m_codec != NULL, Error, , "Could not find decoder for file \""
                << m_mrl << "\"" );
    err = avcodec_open( m_enc, m_codec );
    ERRORMACRO( err >= 0, Error, , "Error opening codec for file \""
                << m_mrl << "\": " << strerror( -err ) );
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
        AVPicture pict;
        m_data = boost::shared_array< char >( new char[ m_enc->width *
                                                        m_enc->height *
                                                        3 / 2 ] );
        pict.data[0] = (uint8_t *)m_data.get();
        pict.data[1] = (uint8_t *)m_data.get() +
                       m_enc->width * m_enc->height * 5 / 4;
        pict.data[2] = (uint8_t *)m_data.get() + m_enc->width * m_enc->height;
        pict.linesize[0] = m_enc->width;
        pict.linesize[1] = m_enc->width / 2;
        pict.linesize[2] = m_enc->width / 2;
        //img_convert(&pict, PIX_FMT_YUV420P,
        //            (AVPicture *)m_frame, m_enc->pix_fmt,
        //            m_enc->width, m_enc->height); // !!!
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
  } catch ( std::exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}
