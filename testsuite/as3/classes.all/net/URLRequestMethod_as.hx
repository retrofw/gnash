// URLRequestMethod_as.hx:  ActionScript 3 "URLRequestMethod" class, for Gnash.
//
// Generated by gen-as3.sh on: 20090514 by "rob". Remove this
// after any hand editing loosing changes.
//
//   Copyright (C) 2009 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

// This test case must be processed by CPP before compiling to include the
//  DejaGnu.hx header file for the testing framework support.

#if flash9
import flash.net.URLRequestMethod;
import flash.display.MovieClip;
#else
import flash.URLRequestMethod;
import flash.MovieClip;
#end
import flash.Lib;
import Type;

// import our testing API
import DejaGnu;

// Class must be named with the _as suffix, as that's the same name as the file.
class URLRequestMethod_as {
    static function main() {

        // Make sure we actually get a valid class
// FIXME: commented out constants are all AIR only	
//         if (URLRequestMethod.DELETE != null) {
//             DejaGnu.pass("URLRequestMethod.DELETE constant exists");
//         } else {
//             DejaGnu.fail("URLRequestMethod.DELETE constant doesn't exist");
//         }

        if (URLRequestMethod.GET != null) {
            DejaGnu.pass("URLRequestMethod.GET constant exists");
        } else {
            DejaGnu.fail("URLRequestMethod.GET constant doesn't exist");
        }

//         if (URLRequestMethod.HEAD != null) {
//             DejaGnu.pass("URLRequestMethod.HEAD constant exists");
//         } else {
//             DejaGnu.fail("URLRequestMethod.HEAD constant doesn't exist");
//         }

//         if (URLRequestMethod.OPTIONS != null) {
//             DejaGnu.pass("URLRequestMethod.OPTIONS constant exists");
//         } else {
//             DejaGnu.fail("URLRequestMethod.OPTIONS constant doesn't exist");
//         }

        if (URLRequestMethod.POST != null) {
            DejaGnu.pass("URLRequestMethod.POST constant exists");
        } else {
            DejaGnu.fail("URLRequestMethod.POST constant doesn't exist");
        }

//         if (URLRequestMethod.PUT != null) {
//             DejaGnu.pass("URLRequestMethod.PUT constant exists");
//         } else {
//             DejaGnu.fail("URLRequestMethod.PUT constant doesn't exist");
//         }

        // Call this after finishing all tests. It prints out the totals.
        DejaGnu.done();
    }
}

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:

