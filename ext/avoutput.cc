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
extern "C" {
  #include <libavutil/mathematics.h>
}
#include "avoutput.hh"

#if !defined(INT64_C)
#define INT64_C(c) c ## LL
#endif

#define VIDEO_BUF_SIZE ( 16 * FF_MIN_BUFFER_SIZE )
#define AUDIO_BUF_SIZE ( 2 * FF_MIN_BUFFER_SIZE )

// Also see https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/muxing.c

using namespace std;

VALUE AVOutput::cRubyClass = Qnil;

AVOutput::AVOutput( const string &mrl, int videoBitRate, int width, int height,
                    int timeBaseNum, int timeBaseDen, int aspectRatioNum,
                    int aspectRatioDen, enum AVCodecID videoCodecID,
                    int audioBitRate, int sampleRate, int channels,
                    enum AVCodecID audioCodecID) throw (Error):
  m_mrl( mrl ), m_oc( NULL ), m_videoStream( NULL ), m_audioStream( NULL),
  m_videoCodecOpen( false ), m_audioCodecOpen( false ), m_videoBuf( NULL ),
  m_audioBuf( NULL ), m_fileOpen( false ), m_headerWritten( false ),
  m_swsContext( NULL ), m_frame( NULL )
{
  try {
    av_register_all();
    ERRORMACRO(avformat_alloc_output_context2(&m_oc, NULL, NULL, mrl.c_str()) >= 0, Error, ,
               "Error allocating output format context: " << strerror(errno));
    ERRORMACRO(m_oc->oformat->video_codec != AV_CODEC_ID_NONE, Error, ,
                "Output format does not support video");
    AVCodec *videoCodec = avcodec_find_encoder(videoCodecID != AV_CODEC_ID_NONE ? videoCodecID :
                                               m_oc->oformat->video_codec);
    ERRORMACRO(videoCodec != NULL, Error, , "Could not find video codec");
    m_videoStream = avformat_new_stream(m_oc, videoCodec);
    ERRORMACRO( m_videoStream != NULL, Error, , "Could not allocate video stream" );
    m_videoStream->id = m_oc->nb_streams - 1;
    m_videoStream->sample_aspect_ratio.num = aspectRatioNum;
    m_videoStream->sample_aspect_ratio.den = aspectRatioDen;
    AVCodecContext *c = m_videoStream->codec;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->codec_id = videoCodecID != AV_CODEC_ID_NONE ? videoCodecID : m_oc->oformat->video_codec;
    c->bit_rate = videoBitRate;
    c->width = width;
    c->height = height;
    c->time_base.den = timeBaseDen;
    c->time_base.num = timeBaseNum;
    c->gop_size = 12;
    c->pix_fmt = PIX_FMT_YUV420P;
    c->sample_aspect_ratio.num = aspectRatioNum;
    c->sample_aspect_ratio.den = aspectRatioDen;
    if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
      c->max_b_frames = 2;
    if (m_oc->oformat->flags & AVFMT_GLOBALHEADER)
      c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    if ( channels > 0 ) {
      ERRORMACRO(m_oc->oformat->audio_codec != AV_CODEC_ID_NONE, Error, ,
                  "Output format does not support audio");
      AVCodec *audioCodec = avcodec_find_encoder(audioCodecID != AV_CODEC_ID_NONE ? audioCodecID :
                                                 m_oc->oformat->audio_codec);
      ERRORMACRO(audioCodec != NULL, Error, , "Could not find audio codec");
      m_audioStream = avformat_new_stream(m_oc, audioCodec);
      ERRORMACRO( m_audioStream != NULL, Error, , "Could not allocate audio stream" );
      m_audioStream->id = 1;
      AVCodecContext *c = m_audioStream->codec;
      c->codec_type = AVMEDIA_TYPE_AUDIO;
      c->codec_id = audioCodecID != CODEC_ID_NONE ? audioCodecID : m_oc->oformat->audio_codec;
      c->sample_fmt = AV_SAMPLE_FMT_S16;
      c->bit_rate = audioBitRate;
      c->sample_rate = sampleRate;
      c->channels = channels;
      if (m_oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    };
    AVCodec *codec = avcodec_find_encoder( c->codec_id );
    ERRORMACRO( codec != NULL, Error, , "Could not find video codec "
                << c->codec_id );
    ERRORMACRO(avcodec_open2(c, codec, NULL) >= 0, Error, ,
               "Error opening video codec \"" << codec->name << "\": "
               << strerror(errno));
    m_videoCodecOpen = true;
    if (!(m_oc->oformat->flags & AVFMT_RAWPICTURE)) {
      m_videoBuf = (char *)av_malloc(VIDEO_BUF_SIZE);
      ERRORMACRO( m_videoBuf != NULL, Error, ,
                  "Error allocating video output buffer" );
    };
    if ( channels > 0 ) {
      AVCodecContext *c = m_audioStream->codec;
      AVCodec *codec = avcodec_find_encoder(c->codec_id);
      ERRORMACRO(codec != NULL, Error, , "Could not find audio codec "
                 << c->codec_id);
      ERRORMACRO(avcodec_open2(c, codec, NULL) >= 0, Error, ,
                 "Error opening audio codec \"" << codec->name << "\": "
                 << strerror(errno));
      m_audioCodecOpen = true;
      m_audioBuf = (char *)av_malloc(AUDIO_BUF_SIZE);
      ERRORMACRO(m_audioBuf != NULL, Error, ,
                 "Error allocating audio output buffer");
    };
    if (!(m_oc->oformat->flags & AVFMT_NOFILE)) {
      ERRORMACRO(avio_open(&m_oc->pb, mrl.c_str(), AVIO_FLAG_WRITE) >= 0, Error, ,
                  "Could not open \"" << mrl << "\"" );
      m_fileOpen = true;
    };
    ERRORMACRO( avformat_write_header(m_oc, NULL) >= 0, Error, ,
                "Error writing header of video \"" << mrl << "\": "
                << strerror( errno ) );
    m_headerWritten = true;
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
  if ( m_headerWritten ) {
    av_write_trailer( m_oc );
    m_headerWritten = false;
  };
  if ( m_audioStream ) {
    if ( m_audioCodecOpen ) {
      avcodec_close( m_audioStream->codec );
      m_audioCodecOpen = false;
    };
  };
  if ( m_videoStream ) {
    if ( m_videoCodecOpen ) {
      avcodec_close( m_videoStream->codec );
      m_videoCodecOpen = false;
    };
  };
  if ( m_audioBuf ) {
    av_free( m_audioBuf );
    m_audioBuf = NULL;
  };
  if ( m_videoBuf ) {
    av_free( m_videoBuf );
    m_videoBuf = NULL;
  };
  if ( m_oc ) {
    for ( unsigned int i = 0; i < m_oc->nb_streams; i++ ) {
      av_freep( &m_oc->streams[i]->codec );
      av_freep( &m_oc->streams[i] );
    };
    m_audioStream = NULL;
    m_videoStream = NULL;
    if ( m_fileOpen ) {
      avio_close(m_oc->pb);
      m_fileOpen = false;
    };
    av_free( m_oc );
    m_oc = NULL;
  };
}

AVRational AVOutput::videoTimeBase(void) throw (Error)
{
  ERRORMACRO( m_videoStream != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_videoStream->time_base;
}

AVRational AVOutput::audioTimeBase(void) throw (Error)
{
  ERRORMACRO( m_audioStream != NULL, Error, , "Audio \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  return m_audioStream->time_base;
}

int AVOutput::frameSize(void) throw (Error)
{
  ERRORMACRO( m_oc != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  ERRORMACRO( m_audioStream != NULL, Error, , "Video \"" << m_mrl << "\" does not have "
              "an audio stream" );
  return m_audioStream->codec->frame_size;
}

int AVOutput::channels(void) throw (Error)
{
  ERRORMACRO( m_oc != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  ERRORMACRO( m_audioStream != NULL, Error, , "Video \"" << m_mrl << "\" does not have "
              "an audio stream" );
  return m_audioStream->codec->channels;
}

void AVOutput::writeVideo( FramePtr frame ) throw (Error)
{
  ERRORMACRO( m_oc != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  ERRORMACRO( m_videoStream->codec->width == frame->width() &&
              m_videoStream->codec->height == frame->height(), Error, ,
              "Resolution of frame is " << frame->width() << 'x'
              << frame->height() << " but video resolution is "
              << m_videoStream->codec->width << 'x' << m_videoStream->codec->height );
  if ( m_oc->oformat->flags & AVFMT_RAWPICTURE ) {
    ERRORMACRO( false, Error, , "Raw picture encoding not implemented yet" );
  } else {
    AVCodecContext *c = m_videoStream->codec;
    AVFrame picture;
    int
      width   = c->width,
      height  = c->height,
      width2  = ( width  + 1 ) / 2,
      height2 = ( height + 1 ) / 2,
      widtha  = ( width  + 7 ) & ~0x7,
      width2a = ( width2 + 7 ) & ~0x7;
    picture.data[0] = (uint8_t *)frame->data();
    picture.data[2] = (uint8_t *)frame->data() + widtha * height;
    picture.data[1] = (uint8_t *)picture.data[2] + width2a * height2;
    picture.linesize[0] = widtha;
    picture.linesize[1] = width2a;
    picture.linesize[2] = width2a;
    sws_scale( m_swsContext, picture.data, picture.linesize, 0,
               c->height, m_frame->data, m_frame->linesize );
    int packetSize = avcodec_encode_video( c, (uint8_t *)m_videoBuf,
                                           VIDEO_BUF_SIZE, m_frame );
    ERRORMACRO( packetSize >= 0, Error, , "Error encoding video frame" );
    if ( packetSize > 0 ) {
      AVPacket packet;
      av_init_packet( &packet );
      if ( c->coded_frame->pts != AV_NOPTS_VALUE )
        packet.pts = av_rescale_q( c->coded_frame->pts, c->time_base,
                                   m_videoStream->time_base );
      if ( c->coded_frame->key_frame )
        packet.flags |= AV_PKT_FLAG_KEY;
      packet.stream_index = m_videoStream->index;
      packet.data = (uint8_t *)m_videoBuf;
      packet.size = packetSize;
      ERRORMACRO( av_interleaved_write_frame( m_oc, &packet ) >= 0, Error, ,
                  "Error writing video frame of video \"" << m_mrl << "\": "
                  << strerror( errno ) );
    };
  };
}

void AVOutput::writeAudio( SequencePtr frame ) throw (Error)
{
  ERRORMACRO( m_oc != NULL, Error, , "Video \"" << m_mrl << "\" is not open. "
              "Did you call \"close\" before?" );
  ERRORMACRO( m_audioStream != NULL, Error, , "Video \"" << m_mrl << "\" does not have "
              "an audio stream" );
  AVCodecContext *c = m_audioStream->codec;
  ERRORMACRO( frame->size() == c->frame_size * 2 * c->channels, Error, , "Size of "
              "audio frame is " << frame->size() << " bytes (but should be "
              << c->frame_size * 2 * c->channels << " bytes)" );
  int packetSize = avcodec_encode_audio( c, (uint8_t *)m_audioBuf,
                                         AUDIO_BUF_SIZE, (short *)frame->data() );
  ERRORMACRO( packetSize >= 0, Error, , "Error encoding audio frame" );
  if ( packetSize > 0 ) {
    AVPacket packet;
    av_init_packet( &packet );
    if ( c->coded_frame && c->coded_frame->pts != AV_NOPTS_VALUE )
      packet.pts = av_rescale_q( c->coded_frame->pts, c->time_base,
                                 m_audioStream->time_base );
    packet.flags |= AV_PKT_FLAG_KEY;
    packet.stream_index = m_audioStream->index;
    packet.data = (uint8_t *)m_audioBuf;
    packet.size = packetSize;
    ERRORMACRO( av_interleaved_write_frame( m_oc, &packet ) >= 0, Error, ,
                "Error writing audio frame of video \"" << m_mrl << "\": "
                << strerror( errno ) );
  };
}

VALUE AVOutput::registerRubyClass( VALUE rbModule )
{
  cRubyClass = rb_define_class_under( rbModule, "AVOutput", rb_cObject );
  rb_define_singleton_method( cRubyClass, "new",
                              RUBY_METHOD_FUNC( wrapNew ), 13 );
  rb_define_const( cRubyClass, "CODEC_ID_NONE",
                   INT2FIX( CODEC_ID_NONE ) );
  rb_define_const( cRubyClass, "CODEC_ID_MPEG1VIDEO",
                   INT2FIX( CODEC_ID_MPEG1VIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_MPEG2VIDEO",
                   INT2FIX( CODEC_ID_MPEG2VIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_MPEG2VIDEO_XVMC",
                   INT2FIX( CODEC_ID_MPEG2VIDEO_XVMC ) );
  rb_define_const( cRubyClass, "CODEC_ID_H261",
                   INT2FIX( CODEC_ID_H261 ) );
  rb_define_const( cRubyClass, "CODEC_ID_H263",
                   INT2FIX( CODEC_ID_H263 ) );
  rb_define_const( cRubyClass, "CODEC_ID_RV10",
                   INT2FIX( CODEC_ID_RV10 ) );
  rb_define_const( cRubyClass, "CODEC_ID_RV20",
                   INT2FIX( CODEC_ID_RV20 ) );
  rb_define_const( cRubyClass, "CODEC_ID_MJPEG",
                   INT2FIX( CODEC_ID_MJPEG ) );
  rb_define_const( cRubyClass, "CODEC_ID_MJPEGB",
                   INT2FIX( CODEC_ID_MJPEGB ) );
  rb_define_const( cRubyClass, "CODEC_ID_LJPEG",
                   INT2FIX( CODEC_ID_LJPEG ) );
  rb_define_const( cRubyClass, "CODEC_ID_SP5X",
                   INT2FIX( CODEC_ID_SP5X ) );
  rb_define_const( cRubyClass, "CODEC_ID_JPEGLS",
                   INT2FIX( CODEC_ID_JPEGLS ) );
  rb_define_const( cRubyClass, "CODEC_ID_MPEG4",
                   INT2FIX( CODEC_ID_MPEG4 ) );
  rb_define_const( cRubyClass, "CODEC_ID_RAWVIDEO",
                   INT2FIX( CODEC_ID_RAWVIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_MSMPEG4V1",
                   INT2FIX( CODEC_ID_MSMPEG4V1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_MSMPEG4V2",
                   INT2FIX( CODEC_ID_MSMPEG4V2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_MSMPEG4V3",
                   INT2FIX( CODEC_ID_MSMPEG4V3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_WMV1",
                   INT2FIX( CODEC_ID_WMV1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_WMV2",
                   INT2FIX( CODEC_ID_WMV2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_H263P",
                   INT2FIX( CODEC_ID_H263P ) );
  rb_define_const( cRubyClass, "CODEC_ID_H263I",
                   INT2FIX( CODEC_ID_H263I ) );
  rb_define_const( cRubyClass, "CODEC_ID_FLV1",
                   INT2FIX( CODEC_ID_FLV1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_SVQ1",
                   INT2FIX( CODEC_ID_SVQ1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_SVQ3",
                   INT2FIX( CODEC_ID_SVQ3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_DVVIDEO",
                   INT2FIX( CODEC_ID_DVVIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_HUFFYUV",
                   INT2FIX( CODEC_ID_HUFFYUV ) );
  rb_define_const( cRubyClass, "CODEC_ID_CYUV",
                   INT2FIX( CODEC_ID_CYUV ) );
  rb_define_const( cRubyClass, "CODEC_ID_H264",
                   INT2FIX( CODEC_ID_H264 ) );
  rb_define_const( cRubyClass, "CODEC_ID_INDEO3",
                   INT2FIX( CODEC_ID_INDEO3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_VP3",
                   INT2FIX( CODEC_ID_VP3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_THEORA",
                   INT2FIX( CODEC_ID_THEORA ) );
  rb_define_const( cRubyClass, "CODEC_ID_ASV1",
                   INT2FIX( CODEC_ID_ASV1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ASV2",
                   INT2FIX( CODEC_ID_ASV2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_FFV1",
                   INT2FIX( CODEC_ID_FFV1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_4XM",
                   INT2FIX( CODEC_ID_4XM ) );
  rb_define_const( cRubyClass, "CODEC_ID_VCR1",
                   INT2FIX( CODEC_ID_VCR1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_CLJR",
                   INT2FIX( CODEC_ID_CLJR ) );
  rb_define_const( cRubyClass, "CODEC_ID_MDEC",
                   INT2FIX( CODEC_ID_MDEC ) );
  rb_define_const( cRubyClass, "CODEC_ID_ROQ",
                   INT2FIX( CODEC_ID_ROQ ) );
  rb_define_const( cRubyClass, "CODEC_ID_INTERPLAY_VIDEO",
                   INT2FIX( CODEC_ID_INTERPLAY_VIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_XAN_WC3",
                   INT2FIX( CODEC_ID_XAN_WC3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_XAN_WC4",
                   INT2FIX( CODEC_ID_XAN_WC4 ) );
  rb_define_const( cRubyClass, "CODEC_ID_RPZA",
                   INT2FIX( CODEC_ID_RPZA ) );
  rb_define_const( cRubyClass, "CODEC_ID_CINEPAK",
                   INT2FIX( CODEC_ID_CINEPAK ) );
  rb_define_const( cRubyClass, "CODEC_ID_WS_VQA",
                   INT2FIX( CODEC_ID_WS_VQA ) );
  rb_define_const( cRubyClass, "CODEC_ID_MSRLE",
                   INT2FIX( CODEC_ID_MSRLE ) );
  rb_define_const( cRubyClass, "CODEC_ID_MSVIDEO1",
                   INT2FIX( CODEC_ID_MSVIDEO1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_IDCIN",
                   INT2FIX( CODEC_ID_IDCIN ) );
  rb_define_const( cRubyClass, "CODEC_ID_8BPS",
                   INT2FIX( CODEC_ID_8BPS ) );
  rb_define_const( cRubyClass, "CODEC_ID_SMC",
                   INT2FIX( CODEC_ID_SMC ) );
  rb_define_const( cRubyClass, "CODEC_ID_FLIC",
                   INT2FIX( CODEC_ID_FLIC ) );
  rb_define_const( cRubyClass, "CODEC_ID_TRUEMOTION1",
                   INT2FIX( CODEC_ID_TRUEMOTION1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_VMDVIDEO",
                   INT2FIX( CODEC_ID_VMDVIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_MSZH",
                   INT2FIX( CODEC_ID_MSZH ) );
  rb_define_const( cRubyClass, "CODEC_ID_ZLIB",
                   INT2FIX( CODEC_ID_ZLIB ) );
  rb_define_const( cRubyClass, "CODEC_ID_QTRLE",
                   INT2FIX( CODEC_ID_QTRLE ) );
  rb_define_const( cRubyClass, "CODEC_ID_SNOW",
                   INT2FIX( CODEC_ID_SNOW ) );
  rb_define_const( cRubyClass, "CODEC_ID_TSCC",
                   INT2FIX( CODEC_ID_TSCC ) );
  rb_define_const( cRubyClass, "CODEC_ID_ULTI",
                   INT2FIX( CODEC_ID_ULTI ) );
  rb_define_const( cRubyClass, "CODEC_ID_QDRAW",
                   INT2FIX( CODEC_ID_QDRAW ) );
  rb_define_const( cRubyClass, "CODEC_ID_VIXL",
                   INT2FIX( CODEC_ID_VIXL ) );
  rb_define_const( cRubyClass, "CODEC_ID_QPEG",
                   INT2FIX( CODEC_ID_QPEG ) );
  rb_define_const( cRubyClass, "CODEC_ID_PNG",
                   INT2FIX( CODEC_ID_PNG ) );
  rb_define_const( cRubyClass, "CODEC_ID_PPM",
                   INT2FIX( CODEC_ID_PPM ) );
  rb_define_const( cRubyClass, "CODEC_ID_PBM",
                   INT2FIX( CODEC_ID_PBM ) );
  rb_define_const( cRubyClass, "CODEC_ID_PGM",
                   INT2FIX( CODEC_ID_PGM ) );
  rb_define_const( cRubyClass, "CODEC_ID_PGMYUV",
                   INT2FIX( CODEC_ID_PGMYUV ) );
  rb_define_const( cRubyClass, "CODEC_ID_PAM",
                   INT2FIX( CODEC_ID_PAM ) );
  rb_define_const( cRubyClass, "CODEC_ID_FFVHUFF",
                   INT2FIX( CODEC_ID_FFVHUFF ) );
  rb_define_const( cRubyClass, "CODEC_ID_RV30",
                   INT2FIX( CODEC_ID_RV30 ) );
  rb_define_const( cRubyClass, "CODEC_ID_RV40",
                   INT2FIX( CODEC_ID_RV40 ) );
  rb_define_const( cRubyClass, "CODEC_ID_VC1",
                   INT2FIX( CODEC_ID_VC1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_WMV3",
                   INT2FIX( CODEC_ID_WMV3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_LOCO",
                   INT2FIX( CODEC_ID_LOCO ) );
  rb_define_const( cRubyClass, "CODEC_ID_WNV1",
                   INT2FIX( CODEC_ID_WNV1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_AASC",
                   INT2FIX( CODEC_ID_AASC ) );
  rb_define_const( cRubyClass, "CODEC_ID_INDEO2",
                   INT2FIX( CODEC_ID_INDEO2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_FRAPS",
                   INT2FIX( CODEC_ID_FRAPS ) );
  rb_define_const( cRubyClass, "CODEC_ID_TRUEMOTION2",
                   INT2FIX( CODEC_ID_TRUEMOTION2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_BMP",
                   INT2FIX( CODEC_ID_BMP ) );
  rb_define_const( cRubyClass, "CODEC_ID_CSCD",
                   INT2FIX( CODEC_ID_CSCD ) );
  rb_define_const( cRubyClass, "CODEC_ID_MMVIDEO",
                   INT2FIX( CODEC_ID_MMVIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_ZMBV",
                   INT2FIX( CODEC_ID_ZMBV ) );
  rb_define_const( cRubyClass, "CODEC_ID_AVS",
                   INT2FIX( CODEC_ID_AVS ) );
  rb_define_const( cRubyClass, "CODEC_ID_SMACKVIDEO",
                   INT2FIX( CODEC_ID_SMACKVIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_NUV",
                   INT2FIX( CODEC_ID_NUV ) );
  rb_define_const( cRubyClass, "CODEC_ID_KMVC",
                   INT2FIX( CODEC_ID_KMVC ) );
  rb_define_const( cRubyClass, "CODEC_ID_FLASHSV",
                   INT2FIX( CODEC_ID_FLASHSV ) );
  rb_define_const( cRubyClass, "CODEC_ID_CAVS",
                   INT2FIX( CODEC_ID_CAVS ) );
  rb_define_const( cRubyClass, "CODEC_ID_JPEG2000",
                   INT2FIX( CODEC_ID_JPEG2000 ) );
  rb_define_const( cRubyClass, "CODEC_ID_VMNC",
                   INT2FIX( CODEC_ID_VMNC ) );
  rb_define_const( cRubyClass, "CODEC_ID_VP5",
                   INT2FIX( CODEC_ID_VP5 ) );
  rb_define_const( cRubyClass, "CODEC_ID_VP6",
                   INT2FIX( CODEC_ID_VP6 ) );
  rb_define_const( cRubyClass, "CODEC_ID_VP6F",
                   INT2FIX( CODEC_ID_VP6F ) );
  rb_define_const( cRubyClass, "CODEC_ID_TARGA",
                   INT2FIX( CODEC_ID_TARGA ) );
  rb_define_const( cRubyClass, "CODEC_ID_DSICINVIDEO",
                   INT2FIX( CODEC_ID_DSICINVIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_TIERTEXSEQVIDEO",
                   INT2FIX( CODEC_ID_TIERTEXSEQVIDEO ) );
  rb_define_const( cRubyClass, "CODEC_ID_TIFF",
                   INT2FIX( CODEC_ID_TIFF ) );
  rb_define_const( cRubyClass, "CODEC_ID_GIF",
                   INT2FIX( CODEC_ID_GIF ) );
  rb_define_const( cRubyClass, "CODEC_ID_DXA",
                   INT2FIX( CODEC_ID_DXA ) );
  rb_define_const( cRubyClass, "CODEC_ID_DNXHD",
                   INT2FIX( CODEC_ID_DNXHD ) );
  rb_define_const( cRubyClass, "CODEC_ID_THP",
                   INT2FIX( CODEC_ID_THP ) );
  rb_define_const( cRubyClass, "CODEC_ID_SGI",
                   INT2FIX( CODEC_ID_SGI ) );
  rb_define_const( cRubyClass, "CODEC_ID_C93",
                   INT2FIX( CODEC_ID_C93 ) );
  rb_define_const( cRubyClass, "CODEC_ID_BETHSOFTVID",
                   INT2FIX( CODEC_ID_BETHSOFTVID ) );
  rb_define_const( cRubyClass, "CODEC_ID_PTX",
                   INT2FIX( CODEC_ID_PTX ) );
  rb_define_const( cRubyClass, "CODEC_ID_TXD",
                   INT2FIX( CODEC_ID_TXD ) );
  rb_define_const( cRubyClass, "CODEC_ID_VP6A",
                   INT2FIX( CODEC_ID_VP6A ) );
  rb_define_const( cRubyClass, "CODEC_ID_AMV",
                   INT2FIX( CODEC_ID_AMV ) );
  rb_define_const( cRubyClass, "CODEC_ID_VB",
                   INT2FIX( CODEC_ID_VB ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCX",
                   INT2FIX( CODEC_ID_PCX ) );
  rb_define_const( cRubyClass, "CODEC_ID_SUNRAST",
                   INT2FIX( CODEC_ID_SUNRAST ) );
  rb_define_const( cRubyClass, "CODEC_ID_INDEO4",
                   INT2FIX( CODEC_ID_INDEO4 ) );
  rb_define_const( cRubyClass, "CODEC_ID_INDEO5",
                   INT2FIX( CODEC_ID_INDEO5 ) );
  rb_define_const( cRubyClass, "CODEC_ID_MIMIC",
                   INT2FIX( CODEC_ID_MIMIC ) );
  rb_define_const( cRubyClass, "CODEC_ID_RL2",
                   INT2FIX( CODEC_ID_RL2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_8SVX_EXP",
                   INT2FIX( CODEC_ID_8SVX_EXP ) );
  rb_define_const( cRubyClass, "CODEC_ID_8SVX_FIB",
                   INT2FIX( CODEC_ID_8SVX_FIB ) );
  rb_define_const( cRubyClass, "CODEC_ID_ESCAPE124",
                   INT2FIX( CODEC_ID_ESCAPE124 ) );
  rb_define_const( cRubyClass, "CODEC_ID_DIRAC",
                   INT2FIX( CODEC_ID_DIRAC ) );
  rb_define_const( cRubyClass, "CODEC_ID_BFI",
                   INT2FIX( CODEC_ID_BFI ) );
  rb_define_const( cRubyClass, "CODEC_ID_CMV",
                   INT2FIX( CODEC_ID_CMV ) );
  rb_define_const( cRubyClass, "CODEC_ID_MOTIONPIXELS",
                   INT2FIX( CODEC_ID_MOTIONPIXELS ) );
  rb_define_const( cRubyClass, "CODEC_ID_TGV",
                   INT2FIX( CODEC_ID_TGV ) );
  rb_define_const( cRubyClass, "CODEC_ID_TGQ",
                   INT2FIX( CODEC_ID_TGQ ) );
  rb_define_const( cRubyClass, "CODEC_ID_TQI",
                   INT2FIX( CODEC_ID_TQI ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S16LE",
                   INT2FIX( CODEC_ID_PCM_S16LE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S16BE",
                   INT2FIX( CODEC_ID_PCM_S16BE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_U16LE",
                   INT2FIX( CODEC_ID_PCM_U16LE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_U16BE",
                   INT2FIX( CODEC_ID_PCM_U16BE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S8",
                   INT2FIX( CODEC_ID_PCM_S8 ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_U8",
                   INT2FIX( CODEC_ID_PCM_U8 ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_MULAW",
                   INT2FIX( CODEC_ID_PCM_MULAW ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_ALAW",
                   INT2FIX( CODEC_ID_PCM_ALAW ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S32LE",
                   INT2FIX( CODEC_ID_PCM_S32LE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S32BE",
                   INT2FIX( CODEC_ID_PCM_S32BE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_U32LE",
                   INT2FIX( CODEC_ID_PCM_U32LE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_U32BE",
                   INT2FIX( CODEC_ID_PCM_U32BE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S24LE",
                   INT2FIX( CODEC_ID_PCM_S24LE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S24BE",
                   INT2FIX( CODEC_ID_PCM_S24BE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_U24LE",
                   INT2FIX( CODEC_ID_PCM_U24LE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_U24BE",
                   INT2FIX( CODEC_ID_PCM_U24BE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S24DAUD",
                   INT2FIX( CODEC_ID_PCM_S24DAUD ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_ZORK",
                   INT2FIX( CODEC_ID_PCM_ZORK ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_S16LE_PLANAR",
                   INT2FIX( CODEC_ID_PCM_S16LE_PLANAR ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_DVD",
                   INT2FIX( CODEC_ID_PCM_DVD ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_F32BE",
                   INT2FIX( CODEC_ID_PCM_F32BE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_F32LE",
                   INT2FIX( CODEC_ID_PCM_F32LE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_F64BE",
                   INT2FIX( CODEC_ID_PCM_F64BE ) );
  rb_define_const( cRubyClass, "CODEC_ID_PCM_F64LE",
                   INT2FIX( CODEC_ID_PCM_F64LE ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_QT",
                   INT2FIX( CODEC_ID_ADPCM_IMA_QT ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_WAV",
                   INT2FIX( CODEC_ID_ADPCM_IMA_WAV ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_DK3",
                   INT2FIX( CODEC_ID_ADPCM_IMA_DK3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_DK4",
                   INT2FIX( CODEC_ID_ADPCM_IMA_DK4 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_WS",
                   INT2FIX( CODEC_ID_ADPCM_IMA_WS ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_SMJPEG",
                   INT2FIX( CODEC_ID_ADPCM_IMA_SMJPEG ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_MS",
                   INT2FIX( CODEC_ID_ADPCM_MS ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_4XM",
                   INT2FIX( CODEC_ID_ADPCM_4XM ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_XA",
                   INT2FIX( CODEC_ID_ADPCM_XA ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_ADX",
                   INT2FIX( CODEC_ID_ADPCM_ADX ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_EA",
                   INT2FIX( CODEC_ID_ADPCM_EA ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_G726",
                   INT2FIX( CODEC_ID_ADPCM_G726 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_CT",
                   INT2FIX( CODEC_ID_ADPCM_CT ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_SWF",
                   INT2FIX( CODEC_ID_ADPCM_SWF ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_YAMAHA",
                   INT2FIX( CODEC_ID_ADPCM_YAMAHA ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_SBPRO_4",
                   INT2FIX( CODEC_ID_ADPCM_SBPRO_4 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_SBPRO_3",
                   INT2FIX( CODEC_ID_ADPCM_SBPRO_3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_SBPRO_2",
                   INT2FIX( CODEC_ID_ADPCM_SBPRO_2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_THP",
                   INT2FIX( CODEC_ID_ADPCM_THP ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_AMV",
                   INT2FIX( CODEC_ID_ADPCM_IMA_AMV ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_EA_R1",
                   INT2FIX( CODEC_ID_ADPCM_EA_R1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_EA_R3",
                   INT2FIX( CODEC_ID_ADPCM_EA_R3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_EA_R2",
                   INT2FIX( CODEC_ID_ADPCM_EA_R2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_EA_SEAD",
                   INT2FIX( CODEC_ID_ADPCM_IMA_EA_SEAD ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_EA_EACS",
                   INT2FIX( CODEC_ID_ADPCM_IMA_EA_EACS ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_EA_XAS",
                   INT2FIX( CODEC_ID_ADPCM_EA_XAS ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_EA_MAXIS_XA",
                   INT2FIX( CODEC_ID_ADPCM_EA_MAXIS_XA ) );
  rb_define_const( cRubyClass, "CODEC_ID_ADPCM_IMA_ISS",
                   INT2FIX( CODEC_ID_ADPCM_IMA_ISS ) );
  rb_define_const( cRubyClass, "CODEC_ID_AMR_NB",
                   INT2FIX( CODEC_ID_AMR_NB ) );
  rb_define_const( cRubyClass, "CODEC_ID_AMR_WB",
                   INT2FIX( CODEC_ID_AMR_WB ) );
  rb_define_const( cRubyClass, "CODEC_ID_RA_144",
                   INT2FIX( CODEC_ID_RA_144 ) );
  rb_define_const( cRubyClass, "CODEC_ID_RA_288",
                   INT2FIX( CODEC_ID_RA_288 ) );
  rb_define_const( cRubyClass, "CODEC_ID_ROQ_DPCM",
                   INT2FIX( CODEC_ID_ROQ_DPCM ) );
  rb_define_const( cRubyClass, "CODEC_ID_INTERPLAY_DPCM",
                   INT2FIX( CODEC_ID_INTERPLAY_DPCM ) );
  rb_define_const( cRubyClass, "CODEC_ID_XAN_DPCM",
                   INT2FIX( CODEC_ID_XAN_DPCM ) );
  rb_define_const( cRubyClass, "CODEC_ID_SOL_DPCM",
                   INT2FIX( CODEC_ID_SOL_DPCM ) );
  rb_define_const( cRubyClass, "CODEC_ID_MP2",
                   INT2FIX( CODEC_ID_MP2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_MP3",
                   INT2FIX( CODEC_ID_MP3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_AAC",
                   INT2FIX( CODEC_ID_AAC ) );
  rb_define_const( cRubyClass, "CODEC_ID_AC3",
                   INT2FIX( CODEC_ID_AC3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_DTS",
                   INT2FIX( CODEC_ID_DTS ) );
  rb_define_const( cRubyClass, "CODEC_ID_VORBIS",
                   INT2FIX( CODEC_ID_VORBIS ) );
  rb_define_const( cRubyClass, "CODEC_ID_DVAUDIO",
                   INT2FIX( CODEC_ID_DVAUDIO ) );
  rb_define_const( cRubyClass, "CODEC_ID_WMAV1",
                   INT2FIX( CODEC_ID_WMAV1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_WMAV2",
                   INT2FIX( CODEC_ID_WMAV2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_MACE3",
                   INT2FIX( CODEC_ID_MACE3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_MACE6",
                   INT2FIX( CODEC_ID_MACE6 ) );
  rb_define_const( cRubyClass, "CODEC_ID_VMDAUDIO",
                   INT2FIX( CODEC_ID_VMDAUDIO ) );
  rb_define_const( cRubyClass, "CODEC_ID_SONIC",
                   INT2FIX( CODEC_ID_SONIC ) );
  rb_define_const( cRubyClass, "CODEC_ID_SONIC_LS",
                   INT2FIX( CODEC_ID_SONIC_LS ) );
  rb_define_const( cRubyClass, "CODEC_ID_FLAC",
                   INT2FIX( CODEC_ID_FLAC ) );
  rb_define_const( cRubyClass, "CODEC_ID_MP3ADU",
                   INT2FIX( CODEC_ID_MP3ADU ) );
  rb_define_const( cRubyClass, "CODEC_ID_MP3ON4",
                   INT2FIX( CODEC_ID_MP3ON4 ) );
  rb_define_const( cRubyClass, "CODEC_ID_SHORTEN",
                   INT2FIX( CODEC_ID_SHORTEN ) );
  rb_define_const( cRubyClass, "CODEC_ID_ALAC",
                   INT2FIX( CODEC_ID_ALAC ) );
  rb_define_const( cRubyClass, "CODEC_ID_WESTWOOD_SND1",
                   INT2FIX( CODEC_ID_WESTWOOD_SND1 ) );
  rb_define_const( cRubyClass, "CODEC_ID_GSM",
                   INT2FIX( CODEC_ID_GSM ) );
  rb_define_const( cRubyClass, "CODEC_ID_QDM2",
                   INT2FIX( CODEC_ID_QDM2 ) );
  rb_define_const( cRubyClass, "CODEC_ID_COOK",
                   INT2FIX( CODEC_ID_COOK ) );
  rb_define_const( cRubyClass, "CODEC_ID_TRUESPEECH",
                   INT2FIX( CODEC_ID_TRUESPEECH ) );
  rb_define_const( cRubyClass, "CODEC_ID_TTA",
                   INT2FIX( CODEC_ID_TTA ) );
  rb_define_const( cRubyClass, "CODEC_ID_SMACKAUDIO",
                   INT2FIX( CODEC_ID_SMACKAUDIO ) );
  rb_define_const( cRubyClass, "CODEC_ID_QCELP",
                   INT2FIX( CODEC_ID_QCELP ) );
  rb_define_const( cRubyClass, "CODEC_ID_WAVPACK",
                   INT2FIX( CODEC_ID_WAVPACK ) );
  rb_define_const( cRubyClass, "CODEC_ID_DSICINAUDIO",
                   INT2FIX( CODEC_ID_DSICINAUDIO ) );
  rb_define_const( cRubyClass, "CODEC_ID_IMC",
                   INT2FIX( CODEC_ID_IMC ) );
  rb_define_const( cRubyClass, "CODEC_ID_MUSEPACK7",
                   INT2FIX( CODEC_ID_MUSEPACK7 ) );
  rb_define_const( cRubyClass, "CODEC_ID_MLP",
                   INT2FIX( CODEC_ID_MLP ) );
  rb_define_const( cRubyClass, "CODEC_ID_GSM_MS",
                   INT2FIX( CODEC_ID_GSM_MS ) );
  rb_define_const( cRubyClass, "CODEC_ID_ATRAC3",
                   INT2FIX( CODEC_ID_ATRAC3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_VOXWARE",
                   INT2FIX( CODEC_ID_VOXWARE ) );
  rb_define_const( cRubyClass, "CODEC_ID_APE",
                   INT2FIX( CODEC_ID_APE ) );
  rb_define_const( cRubyClass, "CODEC_ID_NELLYMOSER",
                   INT2FIX( CODEC_ID_NELLYMOSER ) );
  rb_define_const( cRubyClass, "CODEC_ID_MUSEPACK8",
                   INT2FIX( CODEC_ID_MUSEPACK8 ) );
  rb_define_const( cRubyClass, "CODEC_ID_SPEEX",
                   INT2FIX( CODEC_ID_SPEEX ) );
  rb_define_const( cRubyClass, "CODEC_ID_WMAVOICE",
                   INT2FIX( CODEC_ID_WMAVOICE ) );
  rb_define_const( cRubyClass, "CODEC_ID_WMAPRO",
                   INT2FIX( CODEC_ID_WMAPRO ) );
  rb_define_const( cRubyClass, "CODEC_ID_WMALOSSLESS",
                   INT2FIX( CODEC_ID_WMALOSSLESS ) );
  rb_define_const( cRubyClass, "CODEC_ID_ATRAC3P",
                   INT2FIX( CODEC_ID_ATRAC3P ) );
  rb_define_const( cRubyClass, "CODEC_ID_EAC3",
                   INT2FIX( CODEC_ID_EAC3 ) );
  rb_define_const( cRubyClass, "CODEC_ID_SIPR",
                   INT2FIX( CODEC_ID_SIPR ) );
  rb_define_const( cRubyClass, "CODEC_ID_MP1",
                   INT2FIX( CODEC_ID_MP1 ) );
  rb_define_method( cRubyClass, "close", RUBY_METHOD_FUNC( wrapClose ), 0 );
  rb_define_method( cRubyClass, "video_time_base",
                    RUBY_METHOD_FUNC( wrapVideoTimeBase ), 0 );
  rb_define_method( cRubyClass, "audio_time_base",
                    RUBY_METHOD_FUNC( wrapAudioTimeBase ), 0 );
  rb_define_method( cRubyClass, "frame_size", RUBY_METHOD_FUNC( wrapFrameSize ), 0 );
  rb_define_method( cRubyClass, "channels", RUBY_METHOD_FUNC( wrapChannels ), 0 );
  rb_define_method( cRubyClass, "write_video", RUBY_METHOD_FUNC( wrapWriteVideo ), 1 );
  rb_define_method( cRubyClass, "write_audio", RUBY_METHOD_FUNC( wrapWriteAudio ), 1 );
  return cRubyClass;
}

void AVOutput::deleteRubyObject( void *ptr )
{
  delete (AVOutputPtr *)ptr;
}

VALUE AVOutput::wrapNew(VALUE rbClass, VALUE rbMRL, VALUE rbBitRate, VALUE rbWidth,
                        VALUE rbHeight, VALUE rbTimeBaseNum, VALUE rbTimeBaseDen,
                        VALUE rbAspectRatioNum, VALUE rbAspectRatioDen,
                        VALUE rbVideoCodecID, VALUE rbAudioBitRate, VALUE rbSampleRate,
                        VALUE rbChannels, VALUE rbAudioCodecID)
{
  VALUE retVal = Qnil;
  try {
    rb_check_type( rbMRL, T_STRING );
    AVOutputPtr ptr( new AVOutput( StringValuePtr( rbMRL ), NUM2INT( rbBitRate ),
                                   NUM2INT( rbWidth ), NUM2INT( rbHeight ),
                                   NUM2INT( rbTimeBaseNum ), NUM2INT( rbTimeBaseDen ),
                                   NUM2INT( rbAspectRatioNum ),
                                   NUM2INT( rbAspectRatioDen ),
                                   (enum AVCodecID)NUM2INT(rbVideoCodecID),
                                   NUM2INT( rbAudioBitRate ), NUM2INT( rbSampleRate ),
                                   NUM2INT( rbChannels ),
                                   (enum AVCodecID)NUM2INT(rbAudioCodecID) ) );
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

VALUE AVOutput::wrapVideoTimeBase( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVOutputPtr *self; Data_Get_Struct( rbSelf, AVOutputPtr, self );
    AVRational videoTimeBase = (*self)->videoTimeBase();
    retVal = rb_funcall( rb_cObject, rb_intern( "Rational" ), 2,
                         INT2NUM( videoTimeBase.num ), INT2NUM( videoTimeBase.den ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVOutput::wrapAudioTimeBase( VALUE rbSelf )
{
  VALUE retVal = Qnil;
  try {
    AVOutputPtr *self; Data_Get_Struct( rbSelf, AVOutputPtr, self );
    AVRational audioTimeBase = (*self)->audioTimeBase();
    retVal = rb_funcall( rb_cObject, rb_intern( "Rational" ), 2,
                         INT2NUM( audioTimeBase.num ), INT2NUM( audioTimeBase.den ) );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return retVal;
}

VALUE AVOutput::wrapFrameSize( VALUE rbSelf )
{
  VALUE rbRetVal = Qnil;
  try {
    AVOutputPtr *self; Data_Get_Struct( rbSelf, AVOutputPtr, self );
    rbRetVal = INT2NUM( (*self)->frameSize() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return rbRetVal;
}

VALUE AVOutput::wrapChannels( VALUE rbSelf )
{
  VALUE rbRetVal = Qnil;
  try {
    AVOutputPtr *self; Data_Get_Struct( rbSelf, AVOutputPtr, self );
    rbRetVal = INT2NUM( (*self)->channels() );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return rbRetVal;
}

VALUE AVOutput::wrapWriteVideo( VALUE rbSelf, VALUE rbFrame )
{
  try {
    AVOutputPtr *self; Data_Get_Struct( rbSelf, AVOutputPtr, self );
    FramePtr frame( new Frame( rbFrame ) );
    (*self)->writeVideo( frame );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return rbFrame;
}

VALUE AVOutput::wrapWriteAudio( VALUE rbSelf, VALUE rbFrame )
{
  try {
    AVOutputPtr *self; Data_Get_Struct( rbSelf, AVOutputPtr, self );
    SequencePtr frame( new Sequence( rbFrame ) );
    (*self)->writeAudio( frame );
  } catch ( exception &e ) {
    rb_raise( rb_eRuntimeError, "%s", e.what() );
  };
  return rbFrame;
}

