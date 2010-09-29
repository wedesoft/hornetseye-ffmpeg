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
#include "avoutput.hh"

#define VIDEO_BUF_SIZE 200000

using namespace std;

VALUE AVOutput::cRubyClass = Qnil;

AVOutput::AVOutput( const string &mrl, int bitrate, int width, int height,
                    int timeBaseNum, int timeBaseDen ) throw (Error):
  m_mrl( mrl ), m_oc( NULL ), m_video_st( NULL ), m_video_codec_open( false ),
  m_video_buf( NULL ), m_file_open( false ), m_header_written( false )
{
  try {
    AVOutputFormat *format;
    format = guess_format( NULL, mrl.c_str(), NULL );
    if ( format == NULL ) format = guess_format( "mpeg", NULL, NULL );
    ERRORMACRO( format != NULL, Error, ,
                "Could not find suitable output format for \"" << mrl << "\""  );
    m_oc = avformat_alloc_context();
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
    c->bit_rate = bitrate;
    c->width = width;
    c->height = height;
    c->time_base.num = timeBaseNum;
    c->time_base.den = timeBaseDen;
    c->gop_size = 12;
    c->me_method = 2;
    c->pix_fmt = PIX_FMT_YUV420P;
    if ( m_oc->oformat->flags & AVFMT_GLOBALHEADER )
      c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    ERRORMACRO( av_set_parameters( m_oc, NULL ) >= 0, Error, ,
                "Invalid output format parameters" );
    AVCodec *codec = avcodec_find_encoder( c->codec_id );
    ERRORMACRO( codec != NULL, Error, , "Could not find codec " << c->codec_id );
    ERRORMACRO( avcodec_open( c, codec ) >= 0, Error, ,
                "Error opening codec " << c->codec_id );
    m_video_codec_open = true;
    if ( !( m_oc->oformat->flags & AVFMT_RAWPICTURE ) ) {
      m_video_buf = (char *)av_malloc( VIDEO_BUF_SIZE );
      ERRORMACRO( m_video_buf != NULL, Error, ,
                  "Error allocating video output buffer" );
    };
// alloc_picture
    if ( !( format->flags & AVFMT_NOFILE ) ) {
      ERRORMACRO( url_fopen( &m_oc->pb, mrl.c_str(), URL_WRONLY ) >= 0, Error, ,
                  "Could not open \"" << mrl << "\"" );
      m_file_open = true;
    };
    ERRORMACRO( av_write_header( m_oc ) >= 0, Error, , "Error writing header" );
    m_header_written = true;
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
  if ( m_header_written ) {
    av_write_trailer( m_oc );
    m_header_written = false;
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
    for ( int i = 0; i < m_oc->nb_streams; i++ ) {
      av_freep( &m_oc->streams[i]->codec );
      av_freep( &m_oc->streams[i] );
    };
    m_video_st = NULL;
    if ( m_file_open ) {
      url_fclose( m_oc->pb );
      m_file_open = false;
    };
    av_free( m_oc );
    m_oc = NULL;
  };
}

VALUE AVOutput::registerRubyClass( VALUE rbModule )
{
  cRubyClass = rb_define_class_under( rbModule, "AVOutput", rb_cObject );
  rb_define_singleton_method( cRubyClass, "new",
                              RUBY_METHOD_FUNC( wrapNew ), 6 );
  rb_define_method( cRubyClass, "close", RUBY_METHOD_FUNC( wrapClose ), 0 );
}

void AVOutput::deleteRubyObject( void *ptr )
{
  delete (AVOutputPtr *)ptr;
}

VALUE AVOutput::wrapNew( VALUE rbClass, VALUE rbMRL, VALUE rbBitRate, VALUE rbWidth,
                         VALUE rbHeight, VALUE rbTimeBaseNum, VALUE rbTimeBaseDen )
{
  VALUE retVal = Qnil;
  try {
    rb_check_type( rbMRL, T_STRING );
    AVOutputPtr ptr( new AVOutput( StringValuePtr( rbMRL ), NUM2INT( rbBitRate ),
                                   NUM2INT( rbWidth ), NUM2INT( rbHeight ),
                                   NUM2INT( rbTimeBaseNum ),
                                   NUM2INT( rbTimeBaseDen ) ) );
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
