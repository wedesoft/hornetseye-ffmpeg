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
#include "avoutput.hh"

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#define DLLLOCAL
#else
#define DLLEXPORT __attribute__ ((visibility("default")))
#define DLLLOCAL __attribute__ ((visibility("hidden")))
#endif

extern "C" DLLEXPORT void Init_hornetseye_ffmpeg(void);

extern "C" {

  void Init_hornetseye_ffmpeg(void)
  {
    rb_eval_string( "require 'rational'" );
    rb_eval_string( "require 'hornetseye_frame'" );
    VALUE rbHornetseye = rb_define_module( "Hornetseye" );
    avcodec_register_all();
    av_register_all();
    AVInput::registerRubyClass( rbHornetseye );
    AVOutput::registerRubyClass( rbHornetseye );
    rb_require( "hornetseye_ffmpeg_ext.rb" );
  }

}
