// Sound_as.hx:  ActionScript 3 "Sound" class, for Gnash.
//
// Generated by gen-as3.sh on: 20090503 by "rob". Remove this
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
import flash.media.ID3Info;
import flash.media.Sound;
import flash.media.SoundLoaderContext;
import flash.media.SoundChannel;
import flash.display.MovieClip;
#else
import flash.ID3Info;
import flash.Sound;
import flash.SoundLoaderContext;
import flash.SoundChannel;
import flash.MovieClip;
#end
import flash.Lib;
import Type;

import DejaGnu;

// Class must be named with the PP prefix, as that's the name the
// file passed to haxe will have after the preprocessing step
class Sound_as {
    static function main() {
        var x1:Sound = new Sound();

        // Make sure we actually get a valid class        
        if (x1 != null) {
            DejaGnu.pass("Sound class exists");
        } else {
            DejaGnu.fail("Sound lass doesn't exist");
        }
// Tests to see if all the properties exist. All these do is test for
// existance of a property, and don't test the functionality at all. This
// is primarily useful only to test completeness of the API implementation.
	if (x1.bytesLoaded == 0) {
	    DejaGnu.pass("Sound::bytesLoaded property exists");
	} else {
	    DejaGnu.fail("Sound::bytesLoaded property doesn't exist");
	}
	if (x1.bytesTotal == 0) {
	    DejaGnu.pass("Sound::bytesTotal property exists");
	} else {
	    DejaGnu.fail("Sound::bytesTotal property doesn't exist");
	}
	if (x1.id3 == ID3Info) {
	    DejaGnu.pass("Sound::id3 property exists");
	} else {
	    DejaGnu.fail("Sound::id3 property doesn't exist");
	}
	if (x1.isBuffering == false) {
	    DejaGnu.pass("Sound::isBuffering property exists");
	} else {
	    DejaGnu.fail("Sound::isBuffering property doesn't exist");
	}
	if (x1.length == 0) {
	    DejaGnu.pass("Sound::length property exists");
	} else {
	    DejaGnu.fail("Sound::length property doesn't exist");
	}
	if (x1.url == null) {
	    DejaGnu.pass("Sound::url property exists");
	} else {
	    DejaGnu.fail("Sound::url property doesn't exist");
	}

// Tests to see if all the methods exist. All these do is test for
// existance of a method, and don't test the functionality at all. This
// is primarily useful only to test completeness of the API implementation.
// 	if (x1.Sound == SoundLoaderContext) {
// 	    DejaGnu.pass("Sound::Sound() method exists");
// 	} else {
// 	    DejaGnu.fail("Sound::Sound() method doesn't exist");
// 	}
	if (x1.close == null) {
	    DejaGnu.pass("Sound::close() method exists");
	} else {
	    DejaGnu.fail("Sound::close() method doesn't exist");
	}
	if (x1.load == null) {
	    DejaGnu.pass("Sound::load() method exists");
	} else {
	    DejaGnu.fail("Sound::load() method doesn't exist");
	}
// 	if (x1.play == SoundChannel) {
// 	    DejaGnu.pass("Sound::play() method exists");
// 	} else {
// 	    DejaGnu.fail("Sound::play() method doesn't exist");
// 	}

        // Call this after finishing all tests. It prints out the totals.
        DejaGnu.done();
    }
}
