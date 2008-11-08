// SoundInfo.h: parse and store a SoundInfo record.
//
//   Copyright (C) 2006, 2007, 2008 Free Software Foundation, Inc.
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

#ifndef GNASH_SWF_SOUNDINFO_H
#define GNASH_SWF_SOUNDINFO_H

#include "sound_handler.h" // sound::SoundEnvelopes

namespace gnash {
    class SWFStream;
}

namespace gnash {
namespace SWF {

struct SoundInfo
{
    /// Construct a SoundInfo record object
    //
    /// This SoundInfo is not valid until read() has been called.
    SoundInfo()
        :
        loopCount(0),
        inPoint(0),
        outPoint(0)
    {}

    bool noMultiple;
    bool hasEnvelope;
    bool hasLoops;
    bool hasOutPoint;
    bool hasInPoint;

	/// Number of loops started by an execution of this tag 
	// 
	/// This number is 0 if the sound must be played only once,
	/// 1 to play twice and so on...
	///
	/// It is not known whether a value exists to specify "loop forever"
	///
	int	loopCount;

	/// If true this tag actually *stops* the sound rather then playing it.
	//
	/// In a well-formed SWF when this flag is on all others should be off
	/// (no loops, no envelopes, no in/out points).
	///
	bool stopPlayback;

	/// In/Out points, currently unsupported
	//
	/// See http://sswf.sourceforge.net/SWFalexref.html#swf_soundinfo
    boost::uint32_t inPoint;
    boost::uint32_t outPoint;

	/// Sound effects (envelopes) for this start of the sound
	//
	/// See http://sswf.sourceforge.net/SWFalexref.html#swf_envelope
	///
	sound::SoundEnvelopes envelopes;

    void read(SWFStream& in);

};

}
}

#endif