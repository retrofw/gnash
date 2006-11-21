/* 
 *   Copyright (C) 2005, 2006 Free Software Foundation, Inc.
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
 *
 */ 

#include "MovieTester.h"
#include "GnashException.h"
#include "URL.h"
#include "noseek_fd_adapter.h"
#include "movie_definition.h"
#include "movie_instance.h"
#include "movie_root.h"
#include "sprite_instance.h"
#include "as_environment.h"
#include "gnash.h" // for create_movie and create_library_movie

#include <cstdio>
#include <string>
#include <memory> // for auto_ptr

namespace gnash {

MovieTester::MovieTester(const std::string& url)
{
	if ( url == "-" )
	{
		tu_file* in = noseek_fd_adapter::make_stream(fileno(stdin));
		_movie_def = gnash::create_movie(in, url);
	}
	else
	{
		// _url should be always set at this point...
		_movie_def = gnash::create_library_movie(URL(url));
	}

	// TODO: use PWD if url == '-'
	set_base_url(url);

	if ( ! _movie_def )
	{
		throw GnashException("Could not load movie from "+url);
	}

	// Make sure to load the whole movie
	_movie_def->ensure_frame_loaded(_movie_def->get_frame_count());

	sprite_instance* root = _movie_def->create_instance();
	assert(root);
	_movie_root = dynamic_cast<movie_root*>(root);
	assert(_movie_root);

	// This is surely be needed by internal functions triggered
	// by pressMouseButton and depressMouseButton
	set_current_root(_movie_root);

	_movie = root->get_root_movie();
	assert(_movie);
}

void
MovieTester::advance() 
{
	_movie->advance(1.0);
}

const character*
MovieTester::findDisplayItemByName(const sprite_instance& mc,
		const std::string& name) 
{
	const DisplayList& dlist = mc.getDisplayList();
	return dlist.get_character_by_name(name);
}

const character*
MovieTester::findDisplayItemByDepth(const sprite_instance& mc,
		int depth)
{
	const DisplayList& dlist = mc.getDisplayList();
	return dlist.get_character_at_depth(depth);
}

void
MovieTester::movePointerTo(int x, int y)
{
	_movie_root->notify_mouse_moved(x, y);
}

void
MovieTester::pressMouseButton()
{
	_movie_root->notify_mouse_clicked(true, 1);
}

void
MovieTester::depressMouseButton()
{
	_movie_root->notify_mouse_clicked(false, 1);
}

bool
MovieTester::isMouseOverMouseEntity()
{
	return _movie_root->isMouseOverActiveEntity();
}

} // namespace gnash
