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
#include <cerrno>
#ifndef NDEBUG
#include <iostream>
#endif
#include "avoutput.hh"

#if !defined(INT64_C)
#define INT64_C(c) c ## LL
#endif

#define VIDEO_BUF_SIZE 200000

using namespace std;

VALUE AVOutput::cRubyClass = Qnil;

AVOutput::AVOutput( const string &mrl, int videoBitRate, int width, int height,
                    int timeBaseNum, int timeBaseDen, int audioBitRate,
                    int sampleRate, int channels ) throw (Error):
  m_mrl( mrl ), m_oc( NULL ), m_video_st( NULL ), m_audio_st( NULL),
  m_video_codec_open( false ), m_audio_codec_open( false ), m_video_buf( NULL ),
  m_file_open( false ), m_header_written( false ), m_swsContext( NULL ),
  m_frame( NULL )
{
  try {
    AVOutputFormat *format;
    format = guess_format( NULL, mrl.c_str(), NULL );
    if ( format == NULL ) format = guess_format( "mpeg", NULL, NULL );
    ERRORMACRO( format != NULL, Error, ,
                "Could not find suitable output format for \"" << mrl << "\""  );
#ifdef HAVE_LIBAVFORMAT_ALLOC_CONTEXT
    m_oc = avformat_alloc_context();
#else
    m_oc = av_alloc_format_context();
#endif
    ERRORMACRO( m_oc != NULL, Error, , "Failure allocating format context" );
    m_oc->oformat = format;
    snprintf( m_oc->filename, sizeof( m_oc->filename ), "%s", mrl.c_str() );
    ERRORMACRO( format->video_codec != CODEC_ID_NONE, Error, ,
                "Output format does not support video" );
    m_video_st = av_new_stream( m_oc, 0 );
    ERRORMACRO( m_video_st != NULL, Error, , "Could not allocate video stream" );
    AVCodecContext *c = m_video_st->codec;
    c->codec_id = format->video_codec;
    c->codec_type = CODEC_TYPE_VIDEO;
    c->bit_rate = videoBitRate;
    c->width = width;
    c->height = height;
    c->time_base.num = timeBaseNum;
    c->time_base.den = timeBaseDen;
    c->gop_size = 12;
    c->pix_fmt = PIX_FMT_YUV420P;
    if ( m_oc->oformat->flags & AVFMT_GLOBALHEADER )
      c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    if ( channels > 0 ) {
      m_audio_st = av_new_stream( m_oc, 0 );
      ERRORMACRO( m_audio_st != NULL, Error, , "Could not allocate audio stream" );
      AVCodecContext *c = m_audio_st->codec;
      c->codec_id = format->audio_codec;
      c->codec_type = CODEC_TYPE_AUDIO;
      c->bit_rate = audioBitRate;
      c->sample_rate = sampleRate;
      c->channels = channels;
    };
    ERRORMACRO( av_set_parameters( m_oc, NULL ) >= 0, Error, ,
                "Invalid output format parameters: " << strerror( errno ) );
    AVCodec *codec = avcodec_find_encoder( c->codec_id );
    ERRORMACRO( codec != NULL, Error, , "Could not find video codec "
                << c->codec_id );
    ERRORMACRO( avcodec_open( c, codec ) >= 0, Error, ,
                "Error opening video codec " << c->codec_id << ": "
                << strerror( errno ) );
    m_video_codec_open = true;
    if ( !( m_oc->oformat->flags & AVFMT_RAWPICTURE ) ) {
      m_video_buf = (char *)av_malloc( VIDEO_BUF_SIZE );
      ERRORMACRO( m_video_buf != NULL, Error, ,
                  "Error allocating video output buffer" );
    };
    if ( channels > 0 ) {
      AVCodecContext *c = m_audio_st->codec;
      AVCodec *codec = avcodec_find_encoder( c->codec_id );
      ERRORMACRO( codec != NULL, Error, , "Could not find audio codec "
                  << c->codec_id );
      ERRORMACRO( avcodec_open( c, codec ) >= 0, Error, ,
                  "Error opening audio codec " << c->codec_id << ": "
                  << strerror( errno ) );
      m_audio_codec_open = true;
#ifndef NDEBUG
      cerr << "audio frame size = " << c->frame_size << " samples" << endl;
#endif
    };
    if ( !( format->flags & AVFMT_NOFILE ) ) {
      ERRORMACRO( url_fopen( &m_oc->pb, mrl.c_str(), URL_WRONLY ) >= 0, Error, ,
                  "Could not open \"" << mrl << "\"" );
      m_file_open = true;
    };
    ERRORMACRO( av_write_header( m_oc ) >= 0, Error, ,
                "Error writing header of video \"" << mrl << "\": "
                << strerror( errno ) );
    m_header_written = true;
    m_swsContext = sws_getContext( width, height, PIX_FMT_YUV420P,
                                   width, height, PIX_FMT_YUV420P,
                                   SWS_FAST_BILINEAR, 0, 0, 0 );
    m_frame = avcodec_alloc_frame();
    ERRORMACRO( m_frame, Error, , "Error allocating frame" );
    int size = avpicture_get_size( PIX_FMT_YUV420P, width, height );
    char *frameBuffer = (char *)av_malloc( size );
    ERRORMACRO( frameBuffer, Error, , "Error allocating memory buffer for frame" );
    avpicture_fill( (AVPicture *)m_frame, (uint8_t *)frameBuffer, PIX_FMT_YUV420P,
                    width, height );
  } catch ( Error &e ) {
    close();
    throw e;
  };
}

AVOutput::~AVOutput(void)
{
  close();
}

void AVOutput::close(void)
{
  if ( m_frame ) {
    if ( m_frame->data[0] )
      av_free( m_frame->data[0] );
    av_free( m_frame );
    m_frame = NULL;
  };
  if ( m_swsContext ) {
    sws_freeContext( m_swsContext );
    m_swsContext = NULL;
  };
  if ( m_header_written ) {
    av_write_trailer( m_oc );
    m_header_written = false;
  };
  if ( m_audio_st ) {
    if ( m_audio_codec_open ) {
      avcodec_close( m_audio_st->codec );
      m_audio_codec_open = false;
    };
  };
  if ( m_video_st ) {
    if ( m_video_codec_open ) {
      avcodec_close( m_video_st->codec );
      m_video_codec_open = false;
    };
  };
  if ( m_video_buf ) {
    av_free( m_video_buf );
    m_video_buf = NULL;
  };
  if ( m_oc ) {
    for ( unsigned int i = 0; i < m_oc->nb_streams; i++ ) {
      av_freep( &m_oc->streams[i]->codec );
      av_freep( &m_oc->streams[i] );
    };
    m_audio_st = NULL;
    m_video_st = NULL;
    if ( m_file_open ) {
#ifdef HAVE_BYTEIO_PTR
      url_fclose( m_oc->pb );
#else
      url_fclose( &m_oc->pb );
#endif
      m_file_open = false;
    };
    av_free( m_oc );
    m_oc = NULL;
  };
}

void AVOutput::write( FramePtr frame ) throw (Error)
{
  ERRORMACRO( m_oc != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  ERRORMACRO( m_video_st->codec->width == frame->width() &&
              m_video_st->codec->height == frame->height(), Error, ,
              "Resolution of frame is " << frame->width() << 'x'
              << frame->height() << " but video resolution is "
              << m_video_st->codec->width << 'x' << m_video_st->codec->height );
  if ( m_oc->oformat->flags & AVFMT_RAWPICTURE ) {
    ERRORMACRO( false, Error, , "Raw picture encoding not implemented yet" );
  } else {
    AVCodecContext *c = m_video_st->codec;
    AVFrame picture;
    picture.data[0] = (uint8_t *)frame->data();
    picture.data[1] = (uint8_t *)frame->data() + c->width * c->height * 5 / 4;
    picture.data[2] = (uint8_t *)frame->data() + c->width * c->height;
    picture.linesize[0] = c->width;
    picture.linesize[1] = c->width / 2;
    picture.linesize[2] = c->width / 2;
    sws_scale( m_swsContext, picture.data, picture.linesize, 0,
               c->height, m_frame->data, m_frame->linesize );
    int packetSize = avcodec_encode_video( c, (uint8_t *)m_video_buf,
                                           VIDEO_BUF_SIZE, m_frame );
    ERRORMACRO( packetSize >= 0, Error, , "Error encoding frame" );
    if ( packetSize > 0 ) {
      AVPacket packet;
      av_init_packet( &packet );
      if ( c->coded_frame->pts != AV_NOPTS_VALUE )
        packet.pts = av_rescale_q( c->coded_frame->pts, c->time_base,
                                   m_video_st->time_base );
      if ( c->coded_frame->key_frame )
        packet.flags |= PKT_FLAG_KEY;
      packet.stream_index = m_video_st->index;
      packet.data = (uint8_t *)m_video_buf;
      packet.size = packetSize;
      ERRORMACRO( av_interleaved_write_frame( m_oc, &packet ) >= 0, Error, ,
                  "Error writing frame of video \"" << m_mrl << "\": "
                  << strerror( errno ) );
    };
  };
}

VALUE AVOutput::registerRubyClass( VALUE rbModule )
{
  cRubyClass = rb_define_class_under( rbModule, "AVOutput", rb_cObject );
  rb_define_singleton_method( cRubyClass, "new",
                              RUBY_METHOD_FUNC( wrapNew ), 9 );
  rb_define_method( cRubyClass, "close", RUBY_METHOD_FUNC( wrapClose ), 0 );
  rb_define_method( cRubyClass, "write", RUBY_METHOD_FUNC( wrapWrite ), 1 );
}

void AVOutput::deleteRubyObject( void *ptr )
{
  delete (AVOutputPtr *)ptr;
}

VALUE AVOutput::wrapNew( VALUE rbClass, VALUE rbMRL, VALUE rbBitRate, VALUE rbWidth,
                         VALUE rbHeight, VALUE rbTimeBaseNum, VALUE rbTimeBaseDen,
                         VALUE rbAudioBitRate, VALUE rbSampleRate, VALUE rbChannels )
{
  VALUE retVal = Qnil;
  try {
    rb_check_type( rbMRL, T_STRING );
    AVOutputPtr ptr( new AVOutput( StringValuePtr( rbMRL ), NUM2INT( rbBitRate ),
                                   NUM2INT( rbWidth ), NUM2INT( rbHeight ),
                                   NUM2INT( rbTimeBaseNum ), NUM2INT( rbTimeBaseDen ),
                                   NUM2INT( rbAudioBitRate ), NUM2INT( rbSampleRate ),
                                   NUM2INT( rbChannels ) ) );
    retVal = Data_Wrap_Struct( rbClass, 0, deleteRubyObject,
                               new AVOutputPtr( ptr ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVOutput::wrapClose( VALUE rbSelf )
{
  AVOutputPtr *self; Data_Get_Struct( rbSelf, AVOutputPtr, self );
  (*self)->close();
  return rbSelf;
}

VALUE AVOutput::wrapWrite( VALUE rbSelf, VALUE rbFrame )
{
  try {
    AVOutputPtr *self; Data_Get_Struct( rbSelf, AVOutputPtr, self );
    FramePtr frame( new Frame( rbFrame ) );
    (*self)->write( frame );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return rbFrame;
}
