/* 
 *   Copyright (C) 2007 Free Software Foundation, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */ 

/* 
 *  zou lunkai zoulunkai@gmail.com
 *
 */

#include "ming_utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <ming.h>

#define OUTPUT_VERSION 6
#define OUTPUT_FILENAME "key_event_test5.swf"

int
main(int argc, char** argv)
{
  SWFMovie mo;
  SWFMovieClip dejagnuclip;
  SWFShape sh1, sh2;
  SWFDisplayItem it1, it2;
  int i;


  const char *srcdir=".";
  if ( argc>1 ) 
    srcdir=argv[1];
  else
  {
      //fprintf(stderr, "Usage: %s <mediadir>\n", argv[0]);
      //return 1;
  }

  Ming_init();
  mo = newSWFMovieWithVersion(OUTPUT_VERSION);
  SWFMovie_setDimension(mo, 800, 600);
  SWFMovie_setRate (mo, 1);

  // frame1
  dejagnuclip = get_dejagnu_clip((SWFBlock)get_default_font(srcdir), 10, 0, 0, 800, 600);
  SWFMovie_add(mo, (SWFBlock)dejagnuclip);
  add_actions(mo, " hasKeyPressed = false; x1 = 0;" );

  add_actions(mo, " obj = new Object(); "
                  " obj.onKeyDown = function () "
                  " { "
                  "   _root.note('key listener invoked'); "
                  "   _root.x1 = 1; "
                  " };" 
                  " Key.addListener(obj); "
                  // After deleting obj, we still have a key listener kept alive!
                  " delete obj; " );
  SWFMovie_nextFrame(mo); 

  for(i=1; i<10; i++)
  {
    SWFMovie_nextFrame(mo); 
  }

  // Check that the explicity reference('obj') has been deleted.
  // But the object is still alive as a key listener!
  check_equals(mo, "typeof(obj)", "'undefined'");
  
  add_actions(mo, "if(hasKeyPressed)"
                   "{"
                   "  check_equals(_root.x1, 1); "
                   "}" );
                   
  add_actions(mo, "stop(); total();");
  SWFMovie_nextFrame(mo);  
  
  //Output movie
  puts("Saving " OUTPUT_FILENAME );
  SWFMovie_save(mo, OUTPUT_FILENAME);

  return 0;
}
