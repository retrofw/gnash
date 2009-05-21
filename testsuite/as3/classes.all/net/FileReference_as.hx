// FileReference_as.hx:  ActionScript 3 "FileReference" class, for Gnash.
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
import flash.net.FileReference;
import flash.display.MovieClip;
#else
import flash.FileReference;
import flash.MovieClip;
#end
import flash.Lib;
import Type;

// import our testing API
import DejaGnu;

// Class must be named with the _as suffix, as that's the same name as the file.
class FileReference_as {
    static function main() {
        var x1:FileReference = new FileReference();

        // Make sure we actually get a valid class        
        if (x1 != null) {
            DejaGnu.pass("FileReference class exists");
        } else {
            DejaGnu.fail("FileReference class doesn't exist");
        }
// Tests to see if all the properties exist. All these do is test for
// existance of a property, and don't test the functionality at all. This
// is primarily useful only to test completeness of the API implementation.
// 	if (x1.creationDate == Date) {
// 	    DejaGnu.pass("FileReference.creationDate property exists");
// 	} else {
// 	    DejaGnu.fail("FileReference.creationDate property doesn't exist");
// 	}
	if (x1.creator == null) {
	    DejaGnu.pass("FileReference.creator property exists");
	} else {
	    DejaGnu.fail("FileReference.creator property doesn't exist");
	}
// FIXME: returns an array
// 	if (x1.extension == null) {
// 	    DejaGnu.pass("FileReference.extension property exists");
// 	} else {
// 	    DejaGnu.fail("FileReference.extension property doesn't exist");
// 	}

// 	if (x1.modificationDate == Date) {
// 	    DejaGnu.pass("FileReference.modificationDate property exists");
// 	} else {
// 	    DejaGnu.fail("FileReference.modificationDate property doesn't exist");
// 	}
	if (x1.name == null) {
	    DejaGnu.pass("FileReference.name property exists");
	} else {
	    DejaGnu.fail("FileReference.name property doesn't exist");
	}
	if (x1.size == 0) {
	    DejaGnu.pass("FileReference.size property exists");
	} else {
	    DejaGnu.fail("FileReference.size property doesn't exist");
	}
	if (x1.type == null) {
	    DejaGnu.pass("FileReference.type property exists");
	} else {
	    DejaGnu.fail("FileReference.type property doesn't exist");
	}

// Tests to see if all the methods exist. All these do is test for
// existance of a method, and don't test the functionality at all. This
// is primarily useful only to test completeness of the API implementation.
// FIXME: needs an array
// 	if (x1.browse == false) {
// 	    DejaGnu.pass("FileReference::browse() method exists");
// 	} else {
// 	    DejaGnu.fail("FileReference::browse() method doesn't exist");
// 	}
	if (x1.cancel == null) {
	    DejaGnu.pass("FileReference::cancel() method exists");
	} else {
	    DejaGnu.fail("FileReference::cancel() method doesn't exist");
	}
	if (x1.download == null) {
	    DejaGnu.pass("FileReference::download() method exists");
	} else {
	    DejaGnu.fail("FileReference::download() method doesn't exist");
	}
	if (x1.upload == null) {
	    DejaGnu.pass("FileReference::upload() method exists");
	} else {
	    DejaGnu.fail("FileReference::upload() method doesn't exist");
	}
// This method is AIR only
// 	if (x1.uploadUnencoded == null) {
// 	    DejaGnu.pass("FileReference::uploadUnencoded() method exists");
// 	} else {
// 	    DejaGnu.fail("FileReference::uploadUnencoded() method doesn't exist");
// 	}

        // Call this after finishing all tests. It prints out the totals.
        DejaGnu.done();
    }
}

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:

