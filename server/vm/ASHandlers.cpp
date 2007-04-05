// 
//   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

/* $Id: ASHandlers.cpp,v 1.82 2007/04/05 19:33:54 strk Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "log.h"
#include "swf.h"
#include "rc.h"
#include "ASHandlers.h"
#include "movie_definition.h"
#include "array.h"
#include "swf_function.h"
#include "as_function.h"
#include "tu_random.h"
#include "fn_call.h"
#include "ActionExec.h"
#include "sprite_instance.h"
#include "as_environment.h"
#include "URL.h"
#include "URLAccessManager.h" // for GetUrl actions
#include "action_buffer.h"
#include "as_object.h"
#include "Object.h"
#include "gstring.h" // for automatic as_value::STRING => String as object
#include "Number.h" // for automatic as_value::NUMBER => Number as object
#include "types.h" // for PIXELS_TO_TWIPS
#include "drag_state.h"
#include "VM.h" // for getting the root
#include "movie_root.h" // for set_drag_state (ActionStartDragMovie)
#include "debugger.h"
#include "sound_handler.h"

#include <string>
#include <map>
#include <set>
#include <vector>
#include <utility> // for std::pair

using namespace std;

namespace {
static gnash::LogFile& dbglogfile = gnash::LogFile::getDefaultInstance();
#ifdef USE_DEBUGGER
static gnash::Debugger& debugger = gnash::Debugger::getDefaultInstance();
#endif
}

// Define this to have WaitForFrame actions really
// wait for target frame (and never skip actions)
// See:
//  http://sswf.sourceforge.net/SWFalexref.html#action_wait_for_frame
//
#undef REALLY_WAIT_ON_WAIT_FOR_FRAME

// Forward declarations
namespace gnash {
	extern fscommand_callback s_fscommand_handler;
}

namespace gnash {

namespace SWF { // gnash::SWF

// 
// Utility: construct an object using given constructor.
// This is used by both ActionNew and ActionNewMethod and
// hides differences between builtin and actionscript-defined
// constructors.
//
static boost::intrusive_ptr<as_object>
construct_object(as_function* ctor_as_func,
	as_environment& env, unsigned int nargs,
	unsigned int first_arg_index)
{
	assert(ctor_as_func);
	return ctor_as_func->constructInstance(env, nargs, first_arg_index);
}


static void unsupported_action_handler(ActionExec& thread)
{
	log_error("Unsupported action handler invoked, code at pc is %d", thread.code[thread.pc]);
}

ActionHandler::ActionHandler()
	:
	_name("unsupported"),
	_callback(unsupported_action_handler),
	_debug(false),
	_stack_args(0),
	_arg_format(ARG_NONE)
{
//    GNASH_REPORT_FUNCTION;    
}

ActionHandler::ActionHandler(action_type type, action_callback_t func)
	:
	_type(type),
	_callback(func),
	_debug(false),
	_stack_args(0),
	_arg_format(ARG_NONE)
{
//	GNASH_REPORT_FUNCTION;
}

ActionHandler::ActionHandler(action_type type, string name,
                             action_callback_t func)
	:
	_type(type),
	_name(name),
	_callback(func),
	_debug(false),
	_stack_args(0),
	_arg_format(ARG_NONE)
{
//	GNASH_REPORT_FUNCTION;
}

ActionHandler::ActionHandler(action_type type, string name,
                             action_callback_t func, as_arg_t format)
    : _debug(false), _stack_args(0)
{
//    GNASH_REPORT_FUNCTION;
    _name = name;
    _type = type;
    _callback = func;
    _arg_format = format;
}

ActionHandler::ActionHandler(action_type type, string name,
                             action_callback_t func, as_arg_t format, int nargs)
    : _debug(false)
{
//    GNASH_REPORT_FUNCTION;
    _name = name;
    _type = type;
    _callback = func;
    _stack_args = nargs;
    _arg_format = format;
}

ActionHandler::~ActionHandler()
{
//    GNASH_REPORT_FUNCTION;
}

void
ActionHandler::execute(ActionExec& thread) const
{
//    GNASH_REPORT_FUNCTION;
    return _callback(thread);
}

SWFHandlers::SWFHandlers()
{
//    GNASH_REPORT_FUNCTION;

	// Just to be sure we can start using different handler
	// based on version (would make sense)
	if ( ! VM::isInitialized() )
	{
		log_error("FIXME: VM not initialized at SWFHandlers construction time, can't set action handlers based on SWF version");
	}

    vector<std::string> & property_names = get_property_names();

    property_names.reserve(32);
    property_names.push_back("_x"); // 0
    property_names.push_back("_y"); // 1
    property_names.push_back("_xscale"); // 2
    property_names.push_back("_yscale"); // 3
    property_names.push_back("_currentframe"); // 4
    property_names.push_back("_totalframes"); // 5
    property_names.push_back("_alpha"); // 6
    property_names.push_back("_visible"); // 7
    property_names.push_back("_width"); // 8
    property_names.push_back("_height"); // 9
    property_names.push_back("_rotation"); // 10
    property_names.push_back("_target"); // 11
    property_names.push_back("_framesloaded"); // 12
    property_names.push_back("_name"); // 13
    property_names.push_back("_droptarget"); // 14
    property_names.push_back("_url"); // 15
    property_names.push_back("_highquality"); // 16
    property_names.push_back("_focusrect"); // 17
    property_names.push_back("_soundbuftime"); // 18
    property_names.push_back("@@ mystery quality member"); // 19 - quality: what the quality is (0, 1 or 2)
    property_names.push_back("_xmouse"); // 20
    property_names.push_back("_ymouse"); // 21

    container_type & handlers = get_handlers();
    handlers[ACTION_END] = ActionHandler(ACTION_END,
             string("<End>"), SWFHandlers::ActionEnd);
    handlers[ACTION_NEXTFRAME] = ActionHandler(ACTION_NEXTFRAME,
             string("NextFrame"), SWFHandlers::ActionNextFrame);
    handlers[ACTION_PREVFRAME] =  ActionHandler(ACTION_PREVFRAME,
             string("PreviousFrame"), SWFHandlers::ActionPrevFrame);
    handlers[ACTION_PLAY] = ActionHandler(ACTION_PLAY,
             string("Play"), SWFHandlers::ActionPlay);
    handlers[ACTION_STOP] = ActionHandler(ACTION_STOP,
             string("Stop"), SWFHandlers::ActionStop);
    handlers[ACTION_TOGGLEQUALITY] = ActionHandler(ACTION_TOGGLEQUALITY,
             string("ToggleQuality"), SWFHandlers::ActionToggleQuality);
    handlers[ACTION_STOPSOUNDS] = ActionHandler(ACTION_STOPSOUNDS,
             string("StopSounds"), SWFHandlers::ActionStopSounds);
    handlers[ACTION_GOTOFRAME] = ActionHandler(ACTION_GOTOFRAME,
             string("GotoFrame"), SWFHandlers::ActionGotoFrame, ARG_U16);
    handlers[ACTION_GETURL] = ActionHandler(ACTION_GETURL,
             string("GetUrl"), SWFHandlers::ActionGetUrl, ARG_STR);
    handlers[ACTION_WAITFORFRAME] = ActionHandler(ACTION_WAITFORFRAME,
             string("WaitForFrame"), SWFHandlers::ActionWaitForFrame, ARG_HEX);
    handlers[ACTION_SETTARGET] = ActionHandler(ACTION_SETTARGET,
             string("SetTarget"), SWFHandlers::ActionSetTarget, ARG_STR);
    handlers[ACTION_GOTOLABEL] = ActionHandler(ACTION_GOTOLABEL,
             string("GotoLabel"), SWFHandlers::ActionGotoLabel, ARG_STR);
    handlers[ACTION_ADD] = ActionHandler(ACTION_ADD,
             string("Add"), SWFHandlers::ActionAdd);
    handlers[ACTION_SUBTRACT] = ActionHandler(ACTION_SUBTRACT,
             string("Subtract"), SWFHandlers::ActionSubtract);
    handlers[ACTION_MULTIPLY] = ActionHandler(ACTION_MULTIPLY,
             string("Multiply"), SWFHandlers::ActionMultiply);
    handlers[ACTION_DIVIDE] = ActionHandler(ACTION_DIVIDE,
             string("Divide"), SWFHandlers::ActionDivide);
    handlers[ACTION_EQUAL] = ActionHandler(ACTION_EQUAL,
             string("Equal"), SWFHandlers::ActionEqual);
    handlers[ACTION_LESSTHAN] = ActionHandler(ACTION_LESSTHAN,
             string("LessThan"), SWFHandlers::ActionLessThan);
    handlers[ACTION_LOGICALAND] = ActionHandler(ACTION_LOGICALAND,
             string("LogicalAnd"), SWFHandlers::ActionLogicalAnd);
    handlers[ACTION_LOGICALOR] = ActionHandler(ACTION_LOGICALOR,
             string("LogicalOr"), SWFHandlers::ActionLogicalOr);
    handlers[ACTION_LOGICALNOT] = ActionHandler(ACTION_LOGICALNOT,
             string("LogicalNot"), SWFHandlers::ActionLogicalNot);
    handlers[ACTION_STRINGEQ] = ActionHandler(ACTION_STRINGEQ,
             string("StringEq"), SWFHandlers::ActionStringEq);    
    handlers[ACTION_STRINGLENGTH] = ActionHandler(ACTION_STRINGLENGTH,
             string("ActionStringLength"), SWFHandlers::ActionStringLength);
    handlers[ACTION_SUBSTRING] = ActionHandler(ACTION_SUBSTRING,
             string("ActionSubString"), SWFHandlers::ActionSubString);
    handlers[ACTION_POP] = ActionHandler(ACTION_POP,
             string("ActionPop"), SWFHandlers::ActionPop);
    handlers[ACTION_INT] = ActionHandler(ACTION_INT,
             string("ActionInt"), SWFHandlers::ActionInt);
    handlers[ACTION_GETVARIABLE] = ActionHandler(ACTION_GETVARIABLE,
             string("ActionGetVariable"), SWFHandlers::ActionGetVariable);
    handlers[ACTION_SETVARIABLE] = ActionHandler(ACTION_SETVARIABLE,
             string("ActionSetVariable"), SWFHandlers::ActionSetVariable);
    handlers[ACTION_SETTARGETEXPRESSION] = ActionHandler(ACTION_SETTARGETEXPRESSION,
             string("ActionSetTargetExpression"), SWFHandlers::ActionSetTargetExpression);
    handlers[ACTION_STRINGCONCAT] = ActionHandler(ACTION_STRINGCONCAT,
             string("ActionStringConcat"), SWFHandlers::ActionStringConcat);
    handlers[ACTION_GETPROPERTY] = ActionHandler(ACTION_GETPROPERTY,
             string("ActionGetProperty"), SWFHandlers::ActionGetProperty);
    handlers[ACTION_SETPROPERTY] = ActionHandler(ACTION_SETPROPERTY,
             string("ActionSetpProperty"), SWFHandlers::ActionSetProperty);
    handlers[ACTION_DUPLICATECLIP] = ActionHandler(ACTION_DUPLICATECLIP,
             string("ActionDuplicateClip"), SWFHandlers::ActionDuplicateClip);
    handlers[ACTION_REMOVECLIP] = ActionHandler(ACTION_REMOVECLIP,
             string("ActionRemoveClip"), SWFHandlers::ActionRemoveClip);
    handlers[ACTION_TRACE] = ActionHandler(ACTION_TRACE,
             string("ActionTrace"), SWFHandlers::ActionTrace);
    handlers[ACTION_STARTDRAGMOVIE] = ActionHandler(ACTION_STARTDRAGMOVIE,
             string("ActionStartDragMovie"), SWFHandlers::ActionStartDragMovie);
    handlers[ACTION_STOPDRAGMOVIE] = ActionHandler(ACTION_STOPDRAGMOVIE,
             string("ActionStopDragMovie"), SWFHandlers::ActionStopDragMovie);
    handlers[ACTION_STRINGCOMPARE] = ActionHandler(ACTION_STRINGCOMPARE,
             string("ActionStringCompare"), SWFHandlers::ActionStringCompare);
    handlers[ACTION_THROW] = ActionHandler(ACTION_THROW,
             string("ActionThrow"), SWFHandlers::ActionThrow);
    handlers[ACTION_CASTOP] = ActionHandler(ACTION_CASTOP,
             string("ActionCastOp"), SWFHandlers::ActionCastOp);
    handlers[ACTION_IMPLEMENTSOP] = ActionHandler(ACTION_IMPLEMENTSOP,
             string("ActionImplementsOp"), SWFHandlers::ActionImplementsOp);
    handlers[ACTION_RANDOM] = ActionHandler(ACTION_RANDOM,
             string("ActionRandom"), SWFHandlers::ActionRandom);
    handlers[ACTION_MBLENGTH] = ActionHandler(ACTION_MBLENGTH,
             string("ActionMbLength"), SWFHandlers::ActionMbLength);
    handlers[ACTION_ORD] = ActionHandler(ACTION_ORD,
             string("ActionOrd"), SWFHandlers::ActionOrd);
    handlers[ACTION_CHR] = ActionHandler(ACTION_CHR,
             string("ActionChr"), SWFHandlers::ActionChr);
    handlers[ACTION_GETTIMER] = ActionHandler(ACTION_GETTIMER,
             string("ActionGetTimer"), SWFHandlers::ActionGetTimer);
    handlers[ACTION_MBSUBSTRING] = ActionHandler(ACTION_MBSUBSTRING,
             string("ActionMbSubString"), SWFHandlers::ActionMbSubString);
    handlers[ACTION_MBORD] = ActionHandler(ACTION_MBORD,
             string("ActionMbOrd"), SWFHandlers::ActionMbOrd);
    handlers[ACTION_MBCHR] = ActionHandler(ACTION_MBCHR,
             string("ActionMbChr"), SWFHandlers::ActionMbChr);
    handlers[ACTION_WAITFORFRAMEEXPRESSION] = ActionHandler(ACTION_WAITFORFRAMEEXPRESSION,
             string("ActionWaitForFrameExpression"),
             SWFHandlers::ActionWaitForFrameExpression, ARG_HEX);
    handlers[ACTION_PUSHDATA] = ActionHandler(ACTION_PUSHDATA,
             string("ActionPushData"), SWFHandlers::ActionPushData, ARG_PUSH_DATA);
    handlers[ACTION_BRANCHALWAYS] = ActionHandler(ACTION_BRANCHALWAYS,
             string("ActionBranchAlways"), SWFHandlers::ActionBranchAlways, ARG_S16);
    handlers[ACTION_GETURL2] = ActionHandler(ACTION_GETURL2,
             string("ActionGetUrl2"), SWFHandlers::ActionGetUrl2, ARG_HEX);
    handlers[ACTION_BRANCHIFTRUE] = ActionHandler(ACTION_BRANCHIFTRUE,
             string("ActionBranchIfTrue"), SWFHandlers::ActionBranchIfTrue, ARG_S16);
    handlers[ACTION_CALLFRAME] = ActionHandler(ACTION_CALLFRAME,
             string("ActionCallFrame"), SWFHandlers::ActionCallFrame, ARG_HEX);
    handlers[ACTION_GOTOEXPRESSION] = ActionHandler(ACTION_GOTOEXPRESSION,
             string("ActionGotoExpression"), SWFHandlers::ActionGotoExpression, ARG_HEX);
    handlers[ACTION_DELETE] = ActionHandler(ACTION_DELETE,
             string("ActionDelete"), SWFHandlers::ActionDelete);
    handlers[ACTION_DELETE2] = ActionHandler(ACTION_DELETE2,
             string("ActionDelete2"), SWFHandlers::ActionDelete2);
    handlers[ACTION_VAREQUALS] = ActionHandler(ACTION_VAREQUALS,
             string("ActionVarEquals"), SWFHandlers::ActionVarEquals);
    handlers[ACTION_CALLFUNCTION] = ActionHandler(ACTION_CALLFUNCTION,
             string("ActionCallFunction"), SWFHandlers::ActionCallFunction);
    handlers[ACTION_RETURN] = ActionHandler(ACTION_RETURN,
             string("ActionReturn"), SWFHandlers::ActionReturn);
    handlers[ACTION_MODULO] = ActionHandler(ACTION_MODULO,
             string("ActionModulo"), SWFHandlers::ActionModulo);
    handlers[ACTION_NEW] = ActionHandler(ACTION_NEW,
             string("ActionNew"), SWFHandlers::ActionNew);
    handlers[ACTION_VAR] = ActionHandler(ACTION_VAR,
             string("ActionVar"), SWFHandlers::ActionVar);    
    handlers[ACTION_INITARRAY] = ActionHandler(ACTION_INITARRAY,
             string("ActionInitArray"), SWFHandlers::ActionInitArray);
    handlers[ACTION_INITOBJECT] = ActionHandler(ACTION_INITOBJECT,
             string("ActionInitObject"), SWFHandlers::ActionInitObject);
    handlers[ACTION_TYPEOF] = ActionHandler(ACTION_TYPEOF,
             string("ActionTypeOf"), SWFHandlers::ActionTypeOf);
    handlers[ACTION_TARGETPATH] = ActionHandler(ACTION_TARGETPATH,
             string("ActionTargetPath"), SWFHandlers::ActionTargetPath);
    handlers[ACTION_ENUMERATE] = ActionHandler(ACTION_ENUMERATE,
             string("ActionEnumerate"), SWFHandlers::ActionEnumerate);
    handlers[ACTION_NEWADD] = ActionHandler(ACTION_NEWADD,
             string("ActionNewAdd"), SWFHandlers::ActionNewAdd);
    handlers[ACTION_NEWLESSTHAN] = ActionHandler(ACTION_NEWLESSTHAN,
             string("ActionNewLessThan"), SWFHandlers::ActionNewLessThan);
    handlers[ACTION_NEWEQUALS] = ActionHandler(ACTION_NEWEQUALS,
             string("ActionNewEquals"), SWFHandlers::ActionNewEquals);
    handlers[ACTION_TONUMBER] = ActionHandler(ACTION_TONUMBER,
             string("ActionToNumber"), SWFHandlers::ActionToNumber);
    handlers[ACTION_TOSTRING] = ActionHandler(ACTION_TOSTRING,
             string("ActionToString"), SWFHandlers::ActionToString);
    handlers[ACTION_DUP] = ActionHandler(ACTION_DUP,
             string("ActionDup"), SWFHandlers::ActionDup);    
    handlers[ACTION_SWAP] = ActionHandler(ACTION_SWAP,
             string("ActionSwap"), SWFHandlers::ActionSwap);    
    handlers[ACTION_GETMEMBER] = ActionHandler(ACTION_GETMEMBER,
             string("ActionGetMember"), SWFHandlers::ActionGetMember);
    handlers[ACTION_SETMEMBER] = ActionHandler(ACTION_SETMEMBER,
             string("ActionSetMember"), SWFHandlers::ActionSetMember);
    handlers[ACTION_INCREMENT] = ActionHandler(ACTION_INCREMENT,
             string("ActionIncrement"), SWFHandlers::ActionIncrement);
    handlers[ACTION_DECREMENT] = ActionHandler(ACTION_DECREMENT,
             string("ActionDecrement"), SWFHandlers::ActionDecrement);    
    handlers[ACTION_CALLMETHOD] = ActionHandler(ACTION_CALLMETHOD,
             string("ActionCallMethod"), SWFHandlers::ActionCallMethod);
    handlers[ACTION_NEWMETHOD] = ActionHandler(ACTION_NEWMETHOD,
             string("ActionNewMethod"), SWFHandlers::ActionNewMethod);
    handlers[ACTION_INSTANCEOF] = ActionHandler(ACTION_INSTANCEOF,
             string("ActionInstanceOf"), SWFHandlers::ActionInstanceOf);
    handlers[ACTION_ENUM2] = ActionHandler(ACTION_ENUM2,
             string("ActionEnum2"), SWFHandlers::ActionEnum2);    
    handlers[ACTION_BITWISEAND] = ActionHandler(ACTION_BITWISEAND,
             string("ActionBitwiseAnd"), SWFHandlers::ActionBitwiseAnd);
    handlers[ACTION_BITWISEOR] = ActionHandler(ACTION_BITWISEOR,
             string("ActionBitwiseOr"), SWFHandlers::ActionBitwiseOr);
    handlers[ACTION_BITWISEXOR] = ActionHandler(ACTION_BITWISEXOR,
             string("ActionBitwiseXor"), SWFHandlers::ActionBitwiseXor);
    handlers[ACTION_SHIFTLEFT] = ActionHandler(ACTION_SHIFTLEFT,
             string("ActionShiftLeft"), SWFHandlers::ActionShiftLeft);
    handlers[ACTION_SHIFTRIGHT] = ActionHandler(ACTION_SHIFTRIGHT,
             string("ActionShiftRight"), SWFHandlers::ActionShiftRight);
    handlers[ACTION_SHIFTRIGHT2] = ActionHandler(ACTION_SHIFTRIGHT2,
             string("ActionShiftRight2"), SWFHandlers::ActionShiftRight2);
    handlers[ACTION_STRICTEQ] = ActionHandler(ACTION_STRICTEQ,
             string("ActionStrictEq"), SWFHandlers::ActionStrictEq);
    handlers[ACTION_GREATER] = ActionHandler(ACTION_GREATER,
             string("ActionGreater"), SWFHandlers::ActionGreater);
    handlers[ACTION_STRINGGREATER] = ActionHandler(ACTION_STRINGGREATER,
             string("ActionStringGreater"), SWFHandlers::ActionStringGreater);
    handlers[ACTION_EXTENDS] = ActionHandler(ACTION_EXTENDS,
             string("ActionExtends"), SWFHandlers::ActionExtends);
    handlers[ACTION_CONSTANTPOOL] = ActionHandler(ACTION_CONSTANTPOOL,
             string("ActionConstantPool"), SWFHandlers::ActionConstantPool, ARG_DECL_DICT);
    handlers[ACTION_DEFINEFUNCTION2] = ActionHandler(ACTION_DEFINEFUNCTION2,
             string("ActionDefineFunction2"), SWFHandlers::ActionDefineFunction2,
             ARG_FUNCTION2);
    handlers[ACTION_TRY] = ActionHandler(ACTION_TRY,
             string("ActionTry"), SWFHandlers::ActionTry, ARG_FUNCTION2);
    handlers[ACTION_WITH] = ActionHandler(ACTION_WITH,
             string("ActionWith"), SWFHandlers::ActionWith, ARG_U16);
    handlers[ACTION_DEFINEFUNCTION] = ActionHandler(ACTION_DEFINEFUNCTION,
             string("ActionDefineFunction"), SWFHandlers::ActionDefineFunction, ARG_HEX);
    handlers[ACTION_SETREGISTER] = ActionHandler(ACTION_SETREGISTER,
             string("ActionSetRegister"), SWFHandlers::ActionSetRegister, ARG_U8);
    

}

SWFHandlers::~SWFHandlers()
{
//    GNASH_REPORT_FUNCTION;
}


vector<ActionHandler> &
SWFHandlers::get_handlers()
{
	static container_type handlers(255);
	return handlers;
}

vector<string> &
SWFHandlers::get_property_names()
{
	static vector<string> prop_names;
	return prop_names;
}


const SWFHandlers&
SWFHandlers::instance()
{
	static SWFHandlers instance;
	return instance;
}

void
SWFHandlers::execute(action_type type, ActionExec& thread) const
{
//    GNASH_REPORT_FUNCTION;
//	It is very heavy operation
//	if ( _handlers[type].getName() == "unsupported" ) return false;
	get_handlers()[type].execute(thread);
}

void
SWFHandlers::ActionEnd(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	//as_environment& env = thread.env;
#ifndef NDEBUG
	const action_buffer& code = thread.code;
	assert( code[thread.pc] == SWF::ACTION_END );
#endif

	dbglogfile << __PRETTY_FUNCTION__ << ": CHECKME: was broken" << endl;
	thread.next_pc=thread.stop_pc;
}
void
SWFHandlers::ActionNextFrame(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	assert( code[thread.pc] == SWF::ACTION_NEXTFRAME );

	sprite_instance* tgt = env.get_target()->to_movie();
	assert(tgt);
	tgt->goto_frame(tgt->get_current_frame() + 1);
}

void
SWFHandlers::ActionPrevFrame(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	assert( code[thread.pc] == SWF::ACTION_PREVFRAME );

	sprite_instance* tgt = env.get_target()->to_movie();
	assert(tgt);
	tgt->goto_frame(tgt->get_current_frame() - 1);
}

void
SWFHandlers::ActionPlay(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

    as_environment& env = thread.env;
    const action_buffer& code = thread.code;

    assert( code[thread.pc] == SWF::ACTION_PLAY );
    sprite_instance* tgt = env.get_target()->to_movie();
    assert(tgt);
    tgt->set_play_state(sprite_instance::PLAY);
}

void
SWFHandlers::ActionStop(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

    as_environment& env = thread.env;
    const action_buffer& code = thread.code;

    assert( code[thread.pc] == SWF::ACTION_STOP );
    
    sound_handler* s = get_sound_handler();

    sprite_instance* tgt = env.get_target()->to_movie();
    assert(tgt);
    int stream_id = tgt->get_sound_stream_id();

    if (s != NULL && stream_id != -1)
    {
        s->stop_sound(stream_id);
    }

    tgt->set_play_state(sprite_instance::STOP);
}

void
SWFHandlers::ActionToggleQuality(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

    //as_environment& env = thread.env;
    const action_buffer& code = thread.code;

    assert( code[thread.pc] == SWF::ACTION_TOGGLEQUALITY );
    dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;
}

void
SWFHandlers::ActionStopSounds(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	//as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	assert( code[thread.pc] == SWF::ACTION_STOPSOUNDS );

	sound_handler* s = get_sound_handler();
	if (s != NULL)
	{
		s->stop_all_sounds();
	}
}

void
SWFHandlers::ActionGotoFrame(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	assert( code[thread.pc] == SWF::ACTION_GOTOFRAME );

	size_t frame = code.read_int16(thread.pc+3);

	sprite_instance* tgt = env.get_target()->to_movie();
	assert(tgt);

	// 0-based already?
	//// Convert from 1-based to 0-based
	//frame--;
	tgt->goto_frame(frame);
}

void
SWFHandlers::ActionGetUrl(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	assert( code[thread.pc] == SWF::ACTION_GETURL );

	size_t& pc = thread.pc;

	// If this is an FSCommand, then call the callback
	// handler, if any.
		  
	// Two strings as args.
	// TODO: make sure the NULL terminations are there
	// we could implement a safe_read_string(pc, maxlen)
	// and use tag length as maxlen
	//size_t tag_length = code.read_int16(pc+1);
	const char* url = code.read_string(pc+3);
	size_t url_len = strlen(url)+1;
	const char* target = code.read_string(pc+3+url_len);

		IF_VERBOSE_ACTION (
	log_action("GetUrl: target=%s url=%s", target, url);
		);

	CommonGetUrl(env, target, url, 0u);
}

void
SWFHandlers::ActionWaitForFrame(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	assert( code[thread.pc] == SWF::ACTION_WAITFORFRAME );

	// SWF integrity check
	size_t tag_len = code.read_int16(thread.pc+1);
	if ( tag_len != 3 )
	{
		log_warning("Malformed SWF: ActionWaitForFrame (0x%X) tag length == " SIZET_FMT
		            " (expected 3)", SWF::ACTION_WAITFORFRAME, tag_len);
	}

	// If we haven't loaded a specified frame yet, then 
	// skip the specified number of actions.
	//
	unsigned int framenum = code.read_int16(thread.pc+3);
	uint8 skip = code[thread.pc+5];

	character* target = env.get_target();
	sprite_instance* target_sprite = target->to_movie();
	if ( ! target_sprite )
	{
		log_error("environment target is not a sprite_instance while executing ActionWaitForFrame");
		return;
	}

	// Actually *wait* for target frame, and never skip any action
#ifdef REALLY_WAIT_ON_WAIT_FOR_FRAME
	target_sprite->get_movie_definition()->ensure_frame_loaded(framenum);
	assert(target_sprite->get_loaded_frames() >= framenum);
#endif

	size_t lastloaded = target_sprite->get_loaded_frames();
	if ( lastloaded < framenum )
	{
		//log_msg("ActionWaitForFrame: frame %u not reached yet (loaded %u), skipping next %u actions", framenum, lastloaded, skip);
		// better delegate this to ActionExec
		thread.skip_actions(skip);
	}

	//dbglogfile << __PRETTY_FUNCTION__ << ": testing" << endl;
}

void
SWFHandlers::ActionSetTarget(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	const action_buffer& code = thread.code;
	size_t pc = thread.pc;

	assert(code[pc] == SWF::ACTION_SETTARGET); // 0x8B

	// Change the movie we're working on.
	string target_name ( code.read_string(pc+3) );

	CommonSetTarget(env, target_name);
}

void
SWFHandlers::ActionGotoLabel(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	const char* frame_label = code.read_string(thread.pc+3);
	character *target = env.get_target();
	sprite_instance *target_sprite = target->to_movie();
	if ( ! target_sprite )
	{
		log_error("environment target is not a sprite_instance while executing ActionGotoLabel");
	}
	else
	{
		target_sprite->goto_labeled_frame(frame_label);
	}
}

void
SWFHandlers::ActionAdd(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	//env.top(1) += env.top(0); //original version

	as_environment& env = thread.env;
	thread.ensureStack(2);
	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);
	env.top(1) = operand1 + operand2;
	env.drop(1);
}

void
SWFHandlers::ActionSubtract(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	// env.top(1) -= env.top(0); //original version

	as_environment& env = thread.env;
	thread.ensureStack(2);
	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);
	env.top(1) = operand1 - operand2;
	env.drop(1);
}

void
SWFHandlers::ActionMultiply(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	// env.top(1) *= env.top(0); //original version

	as_environment& env = thread.env;
	thread.ensureStack(2);
	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);
	env.top(1) = operand1 * operand2;
	env.drop(1);
}

void
SWFHandlers::ActionDivide(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
//
	// env.top(1) /= env.top(0); //original version

	as_environment& env = thread.env;
	thread.ensureStack(2);
	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);
	env.top(1) = operand1 / operand2;
	env.drop(1);
}

void
SWFHandlers::ActionEqual(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    assert(thread.code[thread.pc] == SWF::ACTION_EQUAL); // 0x0E

    thread.ensureStack(2);

    as_value& op1 = env.top(0);
    as_value& op2 = env.top(1);

    env.top(1).set_bool(op1.to_number(&env) == op2.to_number(&env));

    // Flash4 used 1 and 0 as return from this tag
    if ( env.get_version() < 5 ) {
      env.top(1).to_number(&env);
    } 

    env.drop(1);
}

void
SWFHandlers::ActionLessThan(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(2);
    env.top(1).set_bool(env.top(1).to_number(&env) < env.top(0).to_number(&env));
    env.drop(1);
}

void
SWFHandlers::ActionLogicalAnd(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(2);
    env.top(1).set_bool(env.top(1).to_bool() && env.top(0).to_bool());
    env.drop(1);
}

void
SWFHandlers::ActionLogicalOr(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(2);
    env.top(1).set_bool(env.top(1).to_bool() || env.top(0).to_bool());
    env.drop(1);
}

void
SWFHandlers::ActionLogicalNot(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1);
    env.top(0).set_bool(! env.top(0).to_bool());
}

void
SWFHandlers::ActionStringEq(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(2);
    env.top(1).set_bool(env.top(1).to_tu_string() == env.top(0).to_tu_string());
    env.drop(1);    
}

void
SWFHandlers::ActionStringLength(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1);
    int version = env.get_version();
    env.top(0).set_int(env.top(0).to_tu_string_versioned(version).utf8_length());
}

void
SWFHandlers::ActionSubString(ActionExec& thread)
{
    //GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(3); // size, base, string

    as_value& size_val = env.top(0);
    as_value& base_val = env.top(1);
    as_value& string_val = env.top(2);

    // input checks
    if ( string_val.is_undefined() || string_val.is_null() )
    {
    	log_warning("Undefined or null string passed to ActionSubString, "
		"returning undefined");
    	env.drop(2);
    	env.top(0).set_undefined();
	return;
    }

    int size = unsigned(size_val.to_number(&env));

    int	base = int(base_val.to_number(&env));  
    int version = env.get_version();
    const tu_string& str = string_val.to_tu_string_versioned(version);

    if ( size < 0 )
    {
    	log_warning("Negative size passed to ActionSubString, "
		"taking as whole length");
	size = str.length();
    }

    // TODO: if 'base' or 'size' do not evaluate to numbers return 
    //       the empty string (how do we check if they evaluate ??)

    if ( base < 1 )
    {
	IF_VERBOSE_ASCODING_ERRORS (
    	log_aserror("Less then 1 base in ActionSubString, "
		"setting to 1.");
	);
	base=1;
    }

    else if ( base > str.length() )
    {
	IF_VERBOSE_ASCODING_ERRORS (
    	log_aserror("base goes beyond input string in ActionSubString, "
		"returning the empty string.");
	);
    	env.drop(2);
    	env.top(0).set_string("");
	return;
    }

    // Base is 1-based, we'll use 0-based from now on...
    base -= 1;

    if ( base+size > str.length() )
    {
	IF_VERBOSE_ASCODING_ERRORS (
    	log_aserror("base+size goes beyond input string in ActionSubString, "
		"adjusting size");
	);
	size = str.length()-base;
    }


    assert(base >= 0);
    assert(base < str.length() );
    assert(size >= 0);

    //log_msg("string: %s, size: %d, base: %d", str.c_str(), size, base);

    // Keep base within range.
    //base = iclamp(base, 0, str.length());
    
    // Truncate if necessary.
    //size = imin(str.length() - base, size);

    // TODO: unsafe: use string::substr instead !
    tu_string	new_string = str.c_str() + base;
    new_string.resize(size);
    
    env.drop(2);
    env.top(0).set_tu_string(new_string);
}

void
SWFHandlers::ActionPop(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;
	// this is an overhead only if SWF is malformed.
	thread.ensureStack(1);
	env.drop(1);
}

void
SWFHandlers::ActionInt(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1);
    env.top(0).set_int(int(floor(env.top(0).to_number(&env))));
}

void
SWFHandlers::ActionGetVariable(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	thread.ensureStack(1); // variable name

	as_value& top_value = env.top(0);
	const char* ptr = top_value.to_string();
	if ( ! ptr )
	{
		top_value.set_undefined();
		return;
	}

	string var_string(ptr);

	top_value = thread.getVariable(var_string);

	IF_VERBOSE_ACTION
	(
	        boost::intrusive_ptr<as_object> obj=top_value.to_object();
		if (obj == NULL) {
			log_action("-- get var: %s=%s",
				var_string.c_str(),
				top_value.to_debug_string().c_str());
		} else {
			log_action("-- get var: %s=%s at %p",
				var_string.c_str(),
				top_value.to_tu_string().c_str(),
				(void*)obj.get());
		}
	);
#ifdef USE_DEBUGGER
	debugger.matchWatchPoint(var_string, Debugger::READS);
#endif
}

void
SWFHandlers::ActionSetVariable(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	// stack must be contain at least two items
	thread.ensureStack(2); 

	assert(env.top(1).to_string());
        string name = env.top(1).to_std_string();
	thread.setVariable(name, env.top(0));

        IF_VERBOSE_ACTION (
            log_action("-- set var: %s = %s", name.c_str(), env.top(0).to_debug_string().c_str());
            );

	// TODO: move this to ActionExec::setVariable !
#ifdef USE_DEBUGGER
	debugger.matchWatchPoint(name, Debugger::WRITES);
#endif

	env.drop(2);
}

// See: http://sswf.sourceforge.net/SWFalexref.html#action_get_dynamic
void
SWFHandlers::ActionSetTargetExpression(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(1);  // target name

	//Vitaly: env.drop(1) remove object on which refers const char * target_name
	//strk: shouldn't we use env.pop() instead ? No (see above comment)
	//const char * target_name = env.top(0).to_string();
	assert(env.top(0).to_string());
	string target_name = env.top(0).to_string();
	env.drop(1); // pop the target name off the stack

	CommonSetTarget(env, target_name);
}

void
SWFHandlers::ActionStringConcat(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(2); // two strings

    int version = env.get_version();
    env.top(1).convert_to_string_versioned(version);
    env.top(1).string_concat(env.top(0).to_tu_string_versioned(version));
    env.drop(1);
}

void
SWFHandlers::ActionGetProperty(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(2); // prop num, target

	as_value& tgt_val = env.top(1);
	std::string tgt_str = tgt_val.to_std_string(&env);
	character *target = NULL;
	if ( tgt_str.empty() )
	{
		as_object* obj = thread.getTarget();

		//log_msg("ActionGetProperty(<empty>) called, target is %p", (void*)obj);

		target = dynamic_cast<character*>(obj);
		if ( ! target )
		{
			log_warning("ActionGetProperty(<empty>) called, but current target is not a character");
		}
	}
	else
	{
		target = env.find_target(tgt_val);
	}
	unsigned int prop_number = (unsigned int)env.top(0).to_number(&env);
	if (target)
	{
		if ( prop_number < get_property_names().size() )
		{
			as_value val;
			// TODO: check if get_propery_names() can return a string
			//       directly.
			assert( get_property_names().size() );
			string propname = get_property_names()[prop_number].c_str();
			//target->get_member(propname &val);
			thread.getObjectMember(*target, propname, val);
			env.top(1) = val;
		}
		else
		{
			log_error("invalid property query, property "
				"number %d", prop_number);
			env.top(1) = as_value();
		}
	}
	else
	{
		// ASCODING error ? (well, last time it was a gnash error ;)
		IF_VERBOSE_ASCODING_ERRORS (
		log_aserror("Could not find GetProperty target (%s)",
				tgt_val.to_debug_string().c_str());
		);
		env.top(1) = as_value();
	}
	env.drop(1);
}

void
SWFHandlers::ActionSetProperty(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    
    thread.ensureStack(3); // prop val, prop num, target

    character *target = env.find_target(env.top(2));
    unsigned int prop_number = (unsigned int)env.top(1).to_number(&env);
    as_value prop_val = env.top(0);
    
    if (target) {
//        set_property(target, prop_number, env.top(0));
        if ( prop_number < get_property_names().size() )
	{
	    // TODO: check if get_property_names() return a string&
	    string member_name = get_property_names()[prop_number].c_str();
	    thread.setObjectMember(*target, member_name, prop_val);
	}
	else
	{
	    // Malformed SWF ? (don't think this is possible to do with syntactically valid ActionScript)
	    log_error("invalid set_property, property number %d", prop_number);
	}
        
    }
    env.drop(3);
}

void
SWFHandlers::ActionDuplicateClip(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(3); 

	sprite_instance* si = env.get_target()->to_movie();
	if ( ! si )
	{
		log_error("environment target is not a sprite_instance while executing ActionDuplicateClip");
	}
	else
	{
		si->clone_display_object(
			env.top(2).to_std_string(),
			env.top(1).to_std_string(),
			(int) env.top(0).to_number(&env));
	}
	env.drop(3);
}

void
SWFHandlers::ActionRemoveClip(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(1); 

	std::string path = env.pop().to_std_string();

	character* ch = env.find_target(path);
	if ( ! ch )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("Path given to removeMovieClip(%s) doesn't point to a character",
			path.c_str());
		);
		return;
	}

	sprite_instance* sprite = ch->to_movie();
	if ( ! sprite )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("Path given to removeMovieClip(%s) is not a sprite",
			path.c_str());
		);
		return;
	}

	sprite_instance* parent = dynamic_cast<sprite_instance*>(sprite->get_parent());
	if (parent)
	{
		parent->remove_display_object(sprite->get_depth(), 0);
	}

}

/// \brief Trace messages from the Flash movie using trace();
void
SWFHandlers::ActionTrace(ActionExec& thread)
{
////    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(1); 

    //std::string val = env.pop().to_std_string_versioned(VM::get().getSWFVersion(), &env);
    std::string val = env.pop().to_std_string(&env);
    log_trace(val.c_str());
}

void
SWFHandlers::ActionStartDragMovie(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    	assert(thread.code[thread.pc] == SWF::ACTION_STARTDRAGMOVIE);

	thread.ensureStack(3); 

	drag_state st;
    
	character* tgt = env.find_target(env.top(0));
	if ( tgt ) {
		st.setCharacter( tgt );
	} else {
		log_error("start_drag of invalid target '%s'.",
			env.top(0).to_debug_string().c_str());
	}
    
	st.setLockCentered( env.top(1).to_bool() );
	if ( env.top(2).to_bool() ) // has bounds !
	{
		// strk: this works if we didn't drop any before, in 
		// a contrary case (if we used pop(), which I suggest)
		// we must remember to updated this as required
		thread.ensureStack(7); // original 3 + 4 for bound

		// It looks like coordinates of drag bounds are
		// given as PIXELS :
		// http://www.richsalter.btinternet.co.uk/cks1/cks1.swf
		float y1 = PIXELS_TO_TWIPS(env.top(3).to_number(&env));
		float x1 = PIXELS_TO_TWIPS(env.top(4).to_number(&env));
		float y0 = PIXELS_TO_TWIPS(env.top(5).to_number(&env));
		float x0 = PIXELS_TO_TWIPS(env.top(6).to_number(&env));

		// check for swapped values
		if ( y1 < y0 )
		{
			IF_VERBOSE_MALFORMED_SWF(
			log_msg("Y values in ActionStartDrag swapped, fixing");
			);
			swap(y1, y0);
		}

		if ( x1 < x0 )
		{
			IF_VERBOSE_MALFORMED_SWF(
			log_msg("X values in ActionStartDrag swapped, fixing");
			);
			swap(x1, x0);
		}

		rect bounds(x0, y0, x1, y1);
		st.setBounds(bounds);

		env.drop(4);
	}

	env.drop(3);
    
	if (tgt)
	{
		VM::get().getRoot().set_drag_state(st);
	}
    
}

void
SWFHandlers::ActionStopDragMovie(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    sprite_instance *root_movie = env.get_target()->get_root_movie();
    assert(root_movie);
    root_movie->stop_drag();
}

void
SWFHandlers::ActionStringCompare(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(2); 
    env.top(1).set_bool(env.top(1).to_tu_string() < env.top(0).to_tu_string());
}

void
SWFHandlers::ActionThrow(ActionExec& /*thread*/)
{
//    GNASH_REPORT_FUNCTION;
    //as_environment& env = thread.env;
    dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;
}

void
SWFHandlers::ActionCastOp(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(2);  // super, instance

	// Get the "super" function
	as_function* super = env.top(0).to_as_function();

	// Get the "instance" 
	boost::intrusive_ptr<as_object> instance = env.top(1).to_object();

	// Invalid args!
	if (!super || ! instance)
	{
		IF_VERBOSE_ACTION (
		log_action("-- %s cast_to %s (invalid args?)",
			env.top(1).to_debug_string().c_str(),
			env.top(0).to_debug_string().c_str());
		);

		env.drop(1);
		env.top(0) = as_value(); 
		return;
	}

	env.drop(1);
	if ( instance->instanceOf(super) )
	{
		env.top(0) = as_value(instance);
	}
	else
	{
		env.top(0) = as_value();
	}

	log_warning("ActionCastOp TESTING");
}

void
SWFHandlers::ActionImplementsOp(ActionExec& /*thread*/)
{
//	GNASH_REPORT_FUNCTION;

	// assert(thread.code[thread.pc] == SWF::ACTION_IMPLEMENTSOP);

	//as_environment& env = thread.env;
	dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;
}

void
SWFHandlers::ActionRandom(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(1);  // max

	int	max = int(env.top(0).to_number(&env));
	if (max < 1) max = 1;
	env.top(0).set_int(tu_random::next_random() % max);
}

void
SWFHandlers::ActionMbLength(ActionExec& /*thread*/)
{
//    GNASH_REPORT_FUNCTION;
//    as_environment& env = thread.env;
    dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;
}

void
SWFHandlers::ActionOrd(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1);  
    env.top(0).set_int(env.top(0).to_string()[0]);
}

void
SWFHandlers::ActionChr(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1);  
    char	buf[2];
    buf[0] = int(env.top(0).to_number(&env));
    buf[1] = 0;
    env.top(0).set_string(buf);
}

void
SWFHandlers::ActionGetTimer(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	// Maybe the timer should be associated to the VM
	// rather then to the movie_root... 
	//
	// Oh, another thing, rather then calling VM::get().getRoot()
	// we should likely get the movie_root from the environment,
	// to take into account a future support for multiple concurrent
	// VM running in a single process (for example: playing multiple
	// movies in multiple windows using the same executable)

	as_environment& env = thread.env;
	env.push(floorf(VM::get().getRoot().get_timer() * 1000.0f));
}

void
SWFHandlers::ActionMbSubString(ActionExec& /*thread*/)
{
//    GNASH_REPORT_FUNCTION;
//    as_environment& env = thread.env;
    dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;
}

void
SWFHandlers::ActionMbOrd(ActionExec& /*thread*/)
{
//    GNASH_REPORT_FUNCTION;
//    as_environment& env = thread.env;
    dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;
}

void
SWFHandlers::ActionMbChr(ActionExec& /*thread*/)
{
//    GNASH_REPORT_FUNCTION;
//    as_environment& env = thread.env;
    dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;
}

// also known as WaitForFrame2
void
SWFHandlers::ActionWaitForFrameExpression(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	thread.ensureStack(1); // expression

	// how many actions to skip if frame has not been loaded
	uint8 skip = code[thread.pc+3];

	// env.top(0) contains frame specification,
	// evaluated as for ActionGotoExpression
	as_value framespec = env.pop();
	
	character* target = env.get_target();
	sprite_instance* target_sprite = target->to_movie();
	if ( ! target_sprite )
	{
		log_error("environment target is not a sprite_instance "
			"while executing ActionWaitForFrameExpression");
		return;
	}

	size_t framenum;
        if ( ! target_sprite->get_frame_number(framespec, framenum) )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("Frame spec found on stack "
			"at ActionWaitForFrame doesn't evaluate "
		        "to a valid frame: %s",
			framespec.to_debug_string().c_str());
		);
		return;
	}

#ifdef REALLY_WAIT_ON_WAIT_FOR_FRAME
	target_sprite->get_movie_definition()->ensure_frame_loaded(framenum);
	assert(target_sprite->get_loaded_frames() >= framenum);
#endif

	size_t lastloaded = target_sprite->get_loaded_frames();
	if ( lastloaded < framenum )
	{
		//log_msg("ActionWaitForFrameExpression: frame %u not reached yet (loaded %u), skipping next %u actions", framenum, lastloaded, skip);
		// better delegate this to ActionExec
		thread.skip_actions(skip);
	}

	//dbglogfile << __PRETTY_FUNCTION__ << ": testing" << endl;
}

void
SWFHandlers::ActionPushData(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	enum {
		pushString,	// 0
		pushFloat,	// 1
		pushNull,	// 2
		pushUndefined,	// 3
		pushRegister,	// 4
		pushBool,	// 5
		pushDouble,	// 6
		pushInt32,	// 7
		pushDict8,	// 8
		pushDict16,	// 9
		pushLast	// 10 - sentinel
	};
	const char* pushType[] = {
		"string",	// 0
		"float",	// 1
		"null",		// 2
		"undefined",	// 3
		"register",	// 4
		"bool",		// 5
		"double",	// 6
		"int32",	// 7
		"dict8",	// 8
		"dict16"	// 9
	};


	const action_buffer& code = thread.code;

	size_t pc = thread.pc;
	int16_t length = code.read_int16(pc+1);
	assert( length >= 0 );

#if 0 // is this really useful ?
        IF_VERBOSE_ACTION (
            log_action("[push length=%d]", length);
            );
#endif

	//---------------
	size_t i = pc;
	size_t count = 0;
	while (i - pc < static_cast<size_t>(length)) {
		int id=0; // for dict (constant pool) lookup
		          // declared here because also used
			  // by verbose action output
		uint8_t type = code[3 + i];
		i++;

		switch (type)
		{
			default:
			{
				IF_VERBOSE_MALFORMED_SWF(
					log_warning("Unknown push type %d."
						" Execution will continue "
						"but it is likely to fail "
						"due to lost sync.", type);
				);
				continue;
			}

			case  pushString: // 0
			{
				const char* str = code.read_string(i+3);
				i += strlen(str) + 1;
				env.push(str);
				break;
			}

			case pushFloat: // 1
			{
				float f = code.read_float_little(i+3);
				i += 4;
				env.push(f);
				break;
			}

			case pushNull: // 2
			{
				as_value nullvalue;
				nullvalue.set_null();
				env.push(nullvalue);	
				break;
			}

			case pushUndefined: // 3
			{
				env.push(as_value());
				break;
			}

			case pushRegister: // 4
			{
				uint8_t reg = code[3 + i];
				++i;
				if ( thread.isFunction2() && reg < env.num_local_registers() )
				{
					env.push(env.local_register(reg));
				}
				else if (reg >= 4)
				{
					env.push(as_value());
					IF_VERBOSE_MALFORMED_SWF(
					log_warning("register %d "
						"out of bounds!", reg);
					);
				}
				else
				{
					env.push(env.global_register(reg));
				}
				break;
			}
		  
			case pushBool: // 5
			{
				bool	bool_val = code[i+3] ? true : false;
				i++;
				env.push(bool_val);
				break;
			}
		  
			case pushDouble: // 6
			{
				double d = code.read_double_wacky(i+3);
				i += 8;
				env.push(d);
				break;
			}
		  
			case pushInt32: // 7
			{
				int32_t val = code.read_int32(i+3);
				i += 4;
				env.push(val);
				break;
			}

			case pushDict8: // 8
			{
				id = code[3 + i];
				i++;
				if ( id < (int) code.dictionary_size() )
				{
					env.push( code.dictionary_get(id) );
				}
				else
				{
					IF_VERBOSE_MALFORMED_SWF(
					log_warning("dict_lookup %d "
					"is out of bounds!", id);
					);
					env.push(0);
				}
				break;
			}

			case pushDict16: // 9
			{
				id = code.read_int16(i+3);
				i += 2;
				if ( id < (int) code.dictionary_size() )
				{
					env.push( code.dictionary_get(id) );
				}
				else
				{
					IF_VERBOSE_MALFORMED_SWF(
					log_warning("dict_lookup %d "
					"is out of bounds!", id);
					);
					env.push(0);
				}
				break;
			}
		}

		IF_VERBOSE_ACTION (
		if ( type == pushDict8 || type == pushDict16 )
		{
			log_action("\t" SIZET_FMT ") type=%s (%d), value=%s", count, pushType[type], id, env.top(0).to_debug_string().c_str());
		}
		else
		{
			log_action("\t" SIZET_FMT ") type=%s, value=%s", count, pushType[type], env.top(0).to_debug_string().c_str());
		}
		++count;
		);
	}
}

void
SWFHandlers::ActionBranchAlways(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	int16_t offset = thread.code.read_int16(thread.pc+3);
	thread.next_pc += offset;
	// @@ TODO range checks
}

// Common code for GetUrl and GetUrl2. See:
// http://sswf.sourceforge.net/SWFalexref.html#action_get_url
// http://sswf.sourceforge.net/SWFalexref.html#action_get_url2
//
// Testcases:
//
// - http://www.garfield.com/comics/comics_todays.html           
//   lower_todayscomic.swf should render four flash files in its canvas
//
// - http://www.voiptalk.org
//   pressing 'My Account' button should open
//   https://www.voiptalk.org/products/login.php
//   NOTE: this is affected by the GetUrl bug reported with an excerpt
//         from Colin Moock book, see below. (won't work, and won't fix)
//
void 
SWFHandlers::CommonGetUrl(as_environment& env,
		as_value target, // the target window, or _level1..10
		const char* url_c,
                uint8_t method /*
				* Bit-packed as follow
				*
                        	* SendVarsMethod:2 (0:NONE 1:GET 2:POST)
                        	* Reserved:4
                        	* LoadTargetFlag:1
                        	* LoadVariableFlag:1
		                */
		)
{

	assert(url_c);

	if ( *url_c == '\0' )
	{
		log_warning("Bogus GetUrl url (empty) in SWF file, skipping");
		return;
	}

#define GETURL2_LOADTARGET_FLAG   1<<7
#define GETURL2_LOADVARIABLE_FLAG 1<<8

	// Parse the method bitfield
	short sendVarsMethod = method & 3;
	bool loadTargetFlag    = method & 64;
	bool loadVariableFlag  = method & 128;

	// handle malformed sendVarsMethod
	if ( sendVarsMethod == 3 )
	{
		log_warning("Bogus GetUrl2 send vars method "
			" in SWF file (both GET and POST requested), set to 0");
		sendVarsMethod=0;
	}

	const char* target_string = NULL;
	if ( ! target.is_undefined() && ! target.is_null() )
	{
		target_string = target.to_string(&env);
	}

	// If the url starts with "FSCommand:", then this is
	// a message for the host app.
	if (strncmp(url_c, "FSCommand:", 10) == 0)
	{
		if (s_fscommand_handler)
		{
			// Call into the app.
			(*s_fscommand_handler)(env.get_target()->get_root_movie(), url_c + 10, target_string);
		}

		return;
	}

	// If the url starts with "print:", then this is
	// a print request.
	if (strncmp(url_c, "print:", 6) == 0)
	{
		log_error("Printing unimplemented");
		return;
	}

	//
	// From "ActionScript: The Definitive Guide" by Colin Moock p. 470
	// --------8<------------------------------------------------------
	// In most browsers, getURL() relative links are resolved relative
	// to the HTML file that contains the .swf file. In IE 4.5 and older
	// versions on Macintosh, relative links are resolved relative to
	// the location of the .swf file, not the HTML file, which causes
	// problems when the two are in different directories. To avoid
	// the problem, either place the .swf and the .html file in the
	// same directory or use absolute URLs when invoking getURL().
	// --------8<------------------------------------------------------
	//
	// We'll resolve relative to our "base url".
	// The base url must be set with the set_base_url() command.
	//

	string url_s(url_c);

	const URL& baseurl = get_base_url();
	URL url(url_s, baseurl);

	log_msg("get url: target=%s, url=%s (%s)", target_string,
		url.str().c_str(), url_c);

	if ( loadTargetFlag )
	{
		character* target_ch = env.find_target(target);
		if ( ! target_ch )
		{
			log_error("get url: target %s not found",
				target_string);
			return;
		}

		sprite_instance* target_movie = target_ch->to_movie();
		if ( ! target_movie ) 
		{
			log_error("get url: target %s is not a sprite",
				target_string);
			return;
		}

		if ( loadVariableFlag )
		{
			log_msg("getURL2 loadVariable");

			//log_warning("Unhandled GetUrl2 loadVariable flag. loadTargetFlag=%d, target=%s (%s)", loadTargetFlag, target.typeOf(), target.to_string());
			target_movie->loadVariables(url, sendVarsMethod);
		}
		else
		{
			log_msg("getURL2 target load");
		      
			// Check host security
			if ( ! URLAccessManager::allow(url) )
			{
				return;
			}


			if ( sendVarsMethod )
			{
				log_warning("Unhandled GetUrl2 sendVariableMethod (%d)"
					" with loadTargetFlag and ! loadVariablesFlag",
					sendVarsMethod);
			}

			sprite_instance* root_movie = env.get_target()->get_root_movie();
			attach_extern_movie(url.str().c_str(), target_movie, root_movie);
		}
	}
	else
	{
		if ( sendVarsMethod )
		{
			log_warning("Unhandled GetUrl2 sendVariableMethod (%d)"
				" with no loadTargetFlag",
				sendVarsMethod);
		}

		string command = "firefox -remote \"openurl(";
		command += url.str();
#if 0 // target testing
		if ( target_string )
		{
			command += ", " + string(target_string);
		}
#endif
		command += ")\"";
		dbglogfile << "Launching URL... " << command << endl;
		system(command.c_str());
	}
}

// Common code for SetTarget and SetTargetExpression. See:
// http://sswf.sourceforge.net/SWFalexref.html#action_set_target
// http://sswf.sourceforge.net/SWFalexref.html#action_get_dynamic
void 
SWFHandlers::CommonSetTarget(as_environment& env, const string& target_name)
{
	character *new_target;
    
	// if the string is blank, we set target to the root movie
	// TODO - double check this is correct?
	if ( target_name.empty() ) {
		new_target = env.find_target(string("/"));
	} else {
		as_value target_val = env.get_variable(target_name);
		new_target = target_val.to_sprite();
	}

	if (new_target == NULL)
	{
		IF_VERBOSE_ASCODING_ERRORS (
		log_aserror(
			"Couldn't find movie \"%s\" to set target to!"
			" Not setting target at all...",
			target_name.c_str());
		);
	}
	else
	{
		env.set_target(new_target);
	}
}

void
SWFHandlers::ActionGetUrl2(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(2); // target, url

	const action_buffer& code = thread.code;

	assert( code[thread.pc] == SWF::ACTION_GETURL2 );

	uint8_t method = code[thread.pc + 3];

	as_value url_val = env.top(1);
	if ( url_val.is_undefined() )
	{
		log_warning("Undefined GetUrl2 url on stack, skipping");
	}
	else
	{
		const char* url = url_val.to_string();
		CommonGetUrl(env, env.top(0), url, method);
	}
		  
	env.drop(2);
}

void
SWFHandlers::ActionBranchIfTrue(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	// Alias these
	as_environment& env = thread.env;
	const action_buffer& code = thread.code;
	size_t& pc = thread.pc;
	size_t& next_pc = thread.next_pc;
	size_t& stop_pc = thread.stop_pc;

	assert( code[pc] == SWF::ACTION_BRANCHIFTRUE );

	thread.ensureStack(1); // bool

	int16_t offset = code.read_int16(pc+3);

	bool test = env.pop().to_bool();
	if (test)
	{
		next_pc += offset;
		      
		if (next_pc > stop_pc)
		{
			log_error("branch to offset " SIZET_FMT "  -- "
				" this section only runs to " SIZET_FMT ". "
				" Malformed SWF !.", next_pc, stop_pc);
		}
	}
}

void
SWFHandlers::ActionCallFrame(ActionExec& thread)
{
        GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(1); // frame spec

	// Note: no extra data in this instruction!
	sprite_instance* tgt = env.get_target()->to_movie();
	assert(tgt);
	tgt->call_frame_actions(env.top(0));
	env.drop(1);
}

void
SWFHandlers::ActionGotoExpression(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(1); // expression

	const action_buffer& code = thread.code;
	size_t pc = thread.pc;

    //dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;

	// From Alexi's SWF ref:
	//
	// Pop a value or a string and jump to the specified
	// frame. When a string is specified, it can include a
	// path to a sprite as in:
	// 
	//   /Test:55
	// 
	// When f_play is ON, the action is to play as soon as
	// that frame is reached. Otherwise, the
	// frame is shown in stop mode.

	unsigned char play_flag = code[pc + 3];
	sprite_instance::play_state state = play_flag ? sprite_instance::PLAY : sprite_instance::STOP;
		  
	sprite_instance* target = env.get_target()->to_movie();
	if ( ! target )
	{
		log_error("environment target is not a sprite_instance while executing ActionGotoExpression");
		env.drop(1);
		return;
	}

	as_value expression = env.pop();

	size_t frame_number;
	if ( ! target->get_frame_number(expression, frame_number) )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("Frame spec found on stack "
			"at ActionGotoExpression doesn't evaluate "
		        "to a valid frame: %s",
			expression.to_debug_string().c_str());
		);
		return;
	}

	target->goto_frame(frame_number);
	target->set_play_state(state);
		  
}


void
SWFHandlers::ActionDelete(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	assert(thread.code[thread.pc] == SWF::ACTION_DELETE); // 0x3A

	size_t stacksize = env.stack_size();

	if ( stacksize < 1 )
	{
		IF_VERBOSE_MALFORMED_SWF(
			log_warning("No elements on the stack "
				"at ActionDelete (0x3A). "
				"Returning FALSE.");
		);
		env.push(as_value(false));
		return;
	}

	as_value var = env.pop();

	bool ret = thread.delVariable(var.to_std_string());
	env.push(as_value(ret));

	return;
}

void
SWFHandlers::ActionDelete2(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	assert(thread.code[thread.pc] == SWF::ACTION_DELETE2); // 0x3B

	thread.ensureStack(1); // var

	// See bug #18482, this works fine now (assuming the bug report is correct)
	env.top(0) = thread.delVariable(env.top(0).to_std_string());
}

void
SWFHandlers::ActionVarEquals(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(2); // value, var

    as_value value = env.pop();
    as_value varname = env.pop();
    thread.setLocalVariable(varname.to_std_string(), value);

    IF_VERBOSE_ACTION (
    log_action("-- set local var: %s = %s", varname.to_string(), value.to_debug_string().c_str());
    );
}

void
SWFHandlers::ActionCallFunction(ActionExec& thread)
{
//        GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;
        string function_name;

	thread.ensureStack(2); // func name, nargs

	//cerr << "At ActionCallFunction enter:"<<endl;
	//env.dump_stack();

	// Let's consider it a as a string and lookup the function.
	as_value function = thread.getVariable(env.top(0).to_std_string(&env));
	if ( ! function.is_object() ) 
	{
		log_aserror("ActionCallFunction: %s is not an object", env.top(0).to_string());
	}
	else if ( ! function.is_function() ) 
	{
		boost::intrusive_ptr<as_object> obj = function.to_object();
		if ( ! obj->get_member("constructor", &function) )
		{
			log_aserror("Object doensn't have a construcor");
		}
	}

	// Get number of args, modifying it if not enough values are on the stack.
	unsigned nargs = unsigned(env.top(1).to_number(&env));
	unsigned available_args = env.stack_size()-2; // 2 for func name and nargs
	if ( available_args < nargs )
	{
		IF_VERBOSE_MALFORMED_SWF(
		log_swferror("Attempt to call a function with %u arguments "
			"while only %u are available on the stack.",
			nargs, available_args);
		);
		nargs = available_args;
	}

	//log_msg("Function's nargs: %d", nargs);
    
#ifdef USE_DEBUGGER
//        cerr << "entering function: " << function_name << endl;
        debugger.callStackPush(function_name);
	debugger.matchBreakPoint(function_name, true);
#endif
	as_value result = call_method(function, &env, thread.getThisPointer(),
				  nargs, env.get_top_index() - 2);

	//log_msg("Function's result: %s", result.to_string());
    
	env.drop(nargs + 1);
	env.top(0) = result;


	//cerr << "After ActionCallFunction:"<<endl;
	//env.dump_stack();
}

void
SWFHandlers::ActionReturn(ActionExec& thread)
{
//        GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;
	as_value* retval = thread.retval;

	//log_msg("Before top/drop (retval=%p)", (void*)retval);
	//env.dump_stack();

	thread.ensureStack(1); // ret value

	// Put top of stack in the provided return slot, if
	// it's not NULL.
	if (retval) {
		*retval = env.top(0);
	}
	env.drop(1);

#ifdef USE_DEBUGGER
//        cerr << "Returning from function: " << env.top(0).to_string() << endl;
        debugger.callStackPop();
#endif

	//log_msg("After top/drop");
	//env.dump_stack();
    
	// Skip the rest of this buffer (return from this action_buffer).
	thread.next_pc = thread.stop_pc;

}

void
SWFHandlers::ActionModulo(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(2); // x, ,y

    as_value	result;
    double	y = env.pop().to_number(&env);
    double	x = env.pop().to_number(&env);
//  Don't need to check for y being 0 here - if it's zero, fmod returns NaN
//  which is what flash would do too
    result = fmod(x, y);
//  env.top(1).set_double(fmod(env.top(1).to_bool() && env.top(0).to_bool());
//  env.drop(1);
//  log_error("modulo x=%f, y=%f, z=%f",x,y,result.to_number(&env));
    env.push(result);
}

void
SWFHandlers::ActionNew(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(2); // classname, nargs

	as_value val = env.pop();
	string classname = val.to_std_string();;

	IF_VERBOSE_ACTION (
		log_action("---new object: %s",
			classname.c_str());
	);

	unsigned nargs = unsigned(env.pop().to_number(&env));

	thread.ensureStack(nargs); // previous 2 entries popped

	as_value constructorval = thread.getVariable(classname); 
	boost::intrusive_ptr<as_function> constructor = constructorval.to_as_function();
	if ( ! constructor )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("ActionNew: "
			"'%s' is NOT a constructor", classname.c_str());
		);
		env.drop(nargs);
		env.push(as_value()); // should we push an object anyway ?
		return;
	}

	boost::intrusive_ptr<as_object> newobj = construct_object(constructor.get(),
			env, nargs, env.get_top_index());

#ifdef USE_DEBUGGER
	// WARNING: new_obj.to_object() can return a newly allocated
	//          thing into the intrusive_ptr, so the debugger
	//          will be left with a deleted object !!
	//          Rob: we don't want to use void pointers here..
	newobj->add_ref(); // this will leak, but at least debugger won't end up
	              	   // with a dandling reference...
        debugger.addSymbol(newobj.get(), classname);
#endif

	env.drop(nargs);
	env.push(as_value(newobj));

}

void
SWFHandlers::ActionVar(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1); // var name
    string varname = env.top(0).to_std_string();
    if ( thread.isFunction() )
    {
       env.declare_local(varname);
    }
    else
    {
       IF_VERBOSE_ASCODING_ERRORS(
       log_aserror("The 'var whatever' syntax in timeline context is a no-op.");
       );
    }
    env.drop(1);
}

void
SWFHandlers::ActionInitArray(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(1); // array size name

    int	array_size = (int) env.pop().to_number(&env);
    assert(array_size >= 0);

    thread.ensureStack((unsigned int)array_size); // array elements
    
    //log_msg("xxx init array: size = %d, top of stack = %d", array_size, env.get_top_index());//xxxxx
    
    // Call the array constructor, to create an empty array.
    as_value	result;
    result = array_new(fn_call(NULL, &env, 0, env.get_top_index()));
    
    boost::intrusive_ptr<as_object> ao = result.to_object();
    assert(ao);
    
    // Fill the elements with the initial values from the stack.
    as_value	index_number;
    for (int i = 0; i < array_size; i++) {
        // @@ TODO a set_member that takes an int or as_value?
        index_number.set_int(i);
        //ao->set_member(index_number.to_string(), env.pop());
        thread.setObjectMember(*ao, index_number.to_std_string(), env.pop());
    }
    
    env.push(result);
    
    //log_msg("xxx init array end: top of stack = %d, trace(top(0)) =", env.get_top_index());//xxxxxxx
    
    //as_global_trace(fn_call(NULL, NULL, env, 1, env.get_top_index()));	//xxxx
    
}

void
SWFHandlers::ActionInitObject(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(1); // nmembers

    // 
    //    SWFACTION_PUSH
    //     [000]   Constant: 1 "obj"
    //     [001]   Constant: 0 "member" <-- we handle up to here
    //     [002]   Integer: 1
    //     [003]   Integer: 1
    //    SWFACTION_INITOBJECT
    
    int nmembers = (int) env.pop().to_number(&env);

    thread.ensureStack(nmembers); // members
    
    boost::intrusive_ptr<as_object> new_obj_ptr(init_object_instance().release()); 
    
    // Set provided members
    for (int i=0; i<nmembers; ++i) {
        as_value member_value = env.pop();
	string member_name = env.pop().to_std_string();
        //new_obj_ptr->set_member(member_name, member_value);
	thread.setObjectMember(*new_obj_ptr, member_name, member_value);
    }
    
    // @@ TODO
    //log_error("checkme opcode: %02X", action_id);
    
    as_value new_obj;
    new_obj.set_as_object(new_obj_ptr.get());
    
    //env.drop(nmembers*2);
    env.push(new_obj); 
    
}

void
SWFHandlers::ActionTypeOf(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(1); 

    env.top(0).set_string(env.top(0).typeOf());
}

void
SWFHandlers::ActionTargetPath(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(1);  // object

	boost::intrusive_ptr<sprite_instance> sp = env.top(0).to_sprite();
	if ( sp )
	{
		env.top(0).set_std_string(sp->getTarget());
	}
	else
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("Argument to TargetPath(%s) doesn't cast to a movieclip",
			env.top(0).to_debug_string().c_str());
		);
		env.top(0).set_undefined();
	}
}

// Push a each object's member value on the stack
// This is an utility function for use by ActionEnumerate
// and ActionEnum2. The caller is expected to have
// already set the top-of-stack to the NULL value (as an optimization)
static void
enumerateObject(as_environment& env, const as_object& obj)
{
//    GNASH_REPORT_FUNCTION;    
	assert( env.top(0).is_null() );
	obj.enumerateProperties(env);
}

void
SWFHandlers::ActionEnumerate(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(1);  // var_name

	// Get the object
	as_value& var_name = env.top(0);
	string var_string = var_name.to_std_string(&env);

	as_value variable = thread.getVariable(var_string);

	env.top(0).set_null();

	if ( ! variable.is_object() )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("Top of stack not an object (%s) at "
			"ActionEnumerate execution",
			variable.to_debug_string().c_str());
		);
		return;
	}

	const boost::intrusive_ptr<as_object> obj = variable.to_object();

	enumerateObject(env, *obj);
}

void
SWFHandlers::ActionNewAdd(ActionExec& thread)
{
    //GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(2); 

    as_value& v1 = env.top(0);
    as_value& v2 = env.top(1);

    //log_msg("ActionNewAdd(%s[%s],%s[%s]) called", v1.typeOf(), v1.to_string(), v2.typeOf(), v2.to_string());


    if (v1.is_string() || v2.is_string() )
    {
    	int version = env.get_version();
        // modify env.top(1)
        v2.convert_to_string_versioned(version, &env);
        v2.string_concat(v1.to_tu_string_versioned(version, &env));
    }
    else
    {
	// use numeric semantic
	double v2num = v2.to_number(&env);
	//log_msg("v2 num = %g", v2num);
	double v1num = v1.to_number(&env);
	//log_msg("v1 num = %g", v1num);

        v2.set_double(v2num + v1num); // modify env.top(1)
    }
    env.drop(1);
}

void
SWFHandlers::ActionNewLessThan(ActionExec& thread)
{
	//GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(2); 

	as_value& operand1 = env.top(1);
	as_value& operand2 = env.top(0);

	if ( operand1.is_string() && operand2.is_string() )
	{
		env.top(1).set_bool(operand1.to_std_string() < operand2.to_std_string());
	}
	else 
	{
		double op1 = operand1.to_number(&env);
		double op2 = operand2.to_number(&env);

		if ( isnan(op1) || isnan(op2) )
		{
			env.top(1).set_undefined();
		}
		else
		{
			env.top(1).set_bool(op1<op2);
		}
	}
	env.drop(1);
}

void
SWFHandlers::ActionNewEquals(ActionExec& thread)
{
    //GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    assert(thread.code[thread.pc] == SWF::ACTION_NEWEQUALS);

    thread.ensureStack(2); 

    /// ECMA-262 abstract equality comparison (sect 11.9.3)
    env.top(1).set_bool(env.top(1).equals(env.top(0), &env));
    env.drop(1);
}

void
SWFHandlers::ActionToNumber(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1); 
    env.top(0).convert_to_number(&env);
}

void
SWFHandlers::ActionToString(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1); 
    int version = env.get_version();
    env.top(0).convert_to_string_versioned(version);
}

void
SWFHandlers::ActionDup(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1); 
    env.push(env.top(0));
}

void
SWFHandlers::ActionSwap(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(2); 
    as_value	temp = env.top(1);
    env.top(1) = env.top(0);
    env.top(0) = temp;
}

void
SWFHandlers::ActionGetMember(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(2); // member name, target

    // Some corner case behaviors depend on the SWF file version.
    int version = env.get_version();
    
    as_value member_name = env.top(0);
    as_value target = env.top(1);
    
    boost::intrusive_ptr<as_object> obj = target.to_object();
    if (!obj) {
//         IF_VERBOSE_DEBUG(log_msg("getMember called against "
//                                  "a value that does not cast "
//                                  "to an as_object: %s", target.to_string()));
        env.top(1).set_undefined();
        env.drop(1);
        return;
    }
    
	IF_VERBOSE_ACTION (
    log_action(" ActionGetMember: target: %s (object %p)",
               target.to_debug_string().c_str(), (void*)obj.get());
	);
    
    // Special case: String has a member "length"
    // @@ FIXME: we shouldn't have all this "special" cases --strk;
    if (target.is_string() && member_name.to_tu_stringi() == "length") {
        int len = target.to_tu_string_versioned(version).utf8_length();
        env.top(1).set_int(len); 
    } else {
        if ( ! thread.getObjectMember(*obj, member_name.to_std_string(), env.top(1)) )
	{
            env.top(1).set_undefined();
        }
        
	IF_VERBOSE_ACTION (
        log_action("-- get_member %s.%s=%s",
		   target.to_debug_string().c_str(),
                   member_name.to_debug_string().c_str(),
                   env.top(1).to_debug_string().c_str());
	);
    }
    env.drop(1);
    
}

void
SWFHandlers::ActionSetMember(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;

	thread.ensureStack(3); // value, member, object

	boost::intrusive_ptr<as_object> obj = env.top(2).to_object();
	string member_name = env.top(1).to_std_string();
	const as_value& member_value = env.top(0);

	if (obj)
	{
		thread.setObjectMember(*obj, member_name, member_value);
		IF_VERBOSE_ACTION (
			log_action("-- set_member %s.%s=%s",
				env.top(2).to_debug_string().c_str(),
				member_name.c_str(),
				member_value.to_debug_string().c_str());
		);
	}
	else
	{
		// Malformed SWF ? (don't think this is possible to do with ActionScript syntax)
		IF_VERBOSE_ACTION (
			// Invalid object, can't set.
			log_action("-- set_member %s.%s=%s on invalid object!",
				env.top(2).to_debug_string().c_str(),
				member_name.c_str(),
				member_value.to_debug_string().c_str());
		);
	}


	env.drop(3);
}

void
SWFHandlers::ActionIncrement(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(1); 

    env.top(0).set_double(env.top(0).to_number(&env)+1);
}

void
SWFHandlers::ActionDecrement(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(1); 
    env.top(0).set_double(env.top(0).to_number(&env)-1);
}

void
SWFHandlers::ActionCallMethod(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(3);  // method_name, obj, nargs

	// Some corner case behaviors depend on the SWF file version.
	//int version = env.get_version();

	// Get name function of the method
	as_value& method_name = env.top(0);

	// Get an object
	as_value& obj_value = env.top(1);

	// Get number of args, modifying it if not enough values are on the stack.
	unsigned nargs = unsigned(env.top(2).to_number(&env));
	unsigned available_args = env.stack_size()-3; // 3 for obj, func and nargs
	if ( available_args < nargs )
	{
		IF_VERBOSE_MALFORMED_SWF(
		log_swferror("Attempt to call a method with %u arguments "
			"while only %u are available on the stack.",
			nargs, available_args);
		);
		nargs = available_args;
	}


	IF_VERBOSE_ACTION (
	log_action(" method name: %s", method_name.to_debug_string().c_str());
	log_action(" method object/func: %s", obj_value.to_debug_string().c_str());
	log_action(" method nargs: %d", nargs);
	);

	string method_string = method_name.to_std_string();
	as_value method_val;
	boost::intrusive_ptr<as_object> obj = obj_value.to_object(); 
	if ( method_name.is_undefined() || method_string.empty() )
	{

		// Does this ever happen ?
		method_val = obj_value;
		if ( ! method_val.is_function() )
		{
			if ( ! obj )
			{
				log_error("ActionCallMethod invoked with "
						"undefined method_name "
						"and non-object object/func");
				env.drop(nargs+2);
				env.top(0).set_undefined();
				return;
			}

			log_warning("Function object given to ActionCallMethod"
				       " is not a function, will try to use"
				       " its 'constructor' member");

			// TODO: all this crap should go into an as_object::getConstructor instead
			as_value ctor;
			if ( ! obj->get_member("constructor", &ctor) )
			{
				IF_VERBOSE_ASCODING_ERRORS(
				log_aserror("ActionCallMethod: object has no constructor");
				);
				env.drop(nargs+2);
				env.top(0).set_undefined();
				return;
			}
			if ( ! ctor.is_function() )
			{
				IF_VERBOSE_ASCODING_ERRORS(
				log_aserror("ActionCallMethod: object constructor is not a function");
				);
				env.drop(nargs+2);
				env.top(0).set_undefined(); 
				return;
			}
			method_val = ctor;
			obj = thread.getThisPointer();
		}
	}
	else
	{
		if ( ! obj )
		{
			// SWF integrity check 
			IF_VERBOSE_ASCODING_ERRORS(
			log_aserror("ActionCallMethod: "
				"Tried to invoke method '%s' on non-object value %s.",
				method_name.to_debug_string().c_str(),
				obj_value.typeOf());
			);
			env.drop(nargs+2);
			env.top(0).set_undefined();
			return;
		}

		if ( ! thread.getObjectMember(*obj, method_string, method_val) )
		{
			IF_VERBOSE_ASCODING_ERRORS(
			log_aserror("ActionCallMethod: "
				"Can't find method %s of object %s",
				method_name.to_debug_string().c_str(),
				obj_value.to_debug_string().c_str());
			);
			env.drop(nargs+2);
			env.top(0).set_undefined(); // should we push an object anyway ?
			return;
		}
	}

#ifdef USE_DEBUGGER
//	cerr << "FIXME: method name is: " << method_name << endl;
//	// IT IS NOT GUARANTEE WE DO HAVE A METHOD NAME HERE !
	if ( ! method_name.is_undefined() )
	{
		debugger.callStackPush(method_name.to_std_string());
		debugger.matchBreakPoint(method_name.to_std_string(), true);
	}
	else
	{
		static bool warned=false;
		if ( ! warned ) {
			log_warning("FIXME: debugger doesn't deal with anonymous function calls");
			warned=true;
		}
	}
#endif

	as_value result = call_method(method_val, &env, obj.get(),
			nargs, env.get_top_index()-3);
    
	env.drop(nargs + 2);
	env.top(0) = result;

	// This is to check stack status after call method
	//log_msg("at doActionCallMethod() end, stack: "); env.dump_stack();
    
}

void
SWFHandlers::ActionNewMethod(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	assert( thread.code[thread.pc] == SWF::ACTION_NEWMETHOD );

	thread.ensureStack(3); // method, object, nargs

	as_value method_name = env.pop();
	as_value obj_val = env.pop();

	// Get number of args, modifying it if not enough values are on the stack.
	unsigned nargs = unsigned(env.pop().to_number(&env));
	unsigned available_args = env.stack_size(); // previous 3 entries popped
	if ( available_args < nargs )
	{
		IF_VERBOSE_MALFORMED_SWF(
		log_swferror("Attempt to call a constructor with %u arguments "
			"while only %u are available on the stack.",
			nargs, available_args);
		);
		nargs = available_args;
	}

	boost::intrusive_ptr<as_object> obj = obj_val.to_object();
	if ( ! obj )
	{
		// SWF integrity check 
		log_warning(
			"On ActionNewMethod: "
			"no object found on stack on ActionMethod");
		env.drop(nargs);
		env.push(as_value());
		return;
	}

	string method_string = method_name.to_std_string();
	as_value method_val;
	if ( method_name.is_undefined() || method_string.empty() )
	{
		method_val = obj_val;
	}
	else
	{
		if ( ! thread.getObjectMember(*obj, method_string, method_val) )
		{
			IF_VERBOSE_ASCODING_ERRORS(
			log_aserror("ActionNewMethod: "
				"can't find method %s of object %s",
				method_string.c_str(), obj_val.to_debug_string().c_str());
			);
			env.drop(nargs);
			env.push(as_value()); // should we push an object anyway ?
			return;
		}
	}

	boost::intrusive_ptr<as_function> method = method_val.to_as_function();
	if ( ! method )
	{
		IF_VERBOSE_MALFORMED_SWF(
		log_swferror("ActionNewMethod: "
			"method name is undefined, "
			"and object is not a function");
		);
		env.drop(nargs);
		env.push(as_value()); // should we push an object anyway ?
		return;
	}

	// Construct the object
	boost::intrusive_ptr<as_object> new_obj = construct_object(method.get(),
			env, nargs, env.get_top_index());

	//log_msg("%s( [%d args] ) returned %s", method_val.to_string(),
	//	nargs, new_obj.to_string());


	env.drop(nargs);
	env.push(as_value(new_obj));

}

void
SWFHandlers::ActionInstanceOf(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;

    thread.ensureStack(2); // super, instance

    // Get the "super" function
    as_function* super = env.top(0).to_as_function();

    // Get the "instance" (but avoid implicit conversion of primitive values!)
    boost::intrusive_ptr<as_object> instance = env.top(1).is_object() ? env.top(1).to_object() : NULL;

    // Invalid args!
    if (!super || ! instance) {
        IF_VERBOSE_ACTION(
        log_action("-- %s instanceof %s (invalid args?)",
                env.top(1).to_debug_string().c_str(),
                env.top(0).to_debug_string().c_str());
        );

        env.drop(1);
        env.top(0) = as_value(false); 
        return;
    }

    env.drop(1);
    env.top(0) = as_value(instance->instanceOf(super));
}

void
SWFHandlers::ActionEnum2(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(1); // object

	// Get the object.
	// Copy it so we can override env.top(0)
	as_value obj_val = env.top(0);

	// End of the enumeration. Won't override the object
	// as we copied that as_value.
	env.top(0).set_null(); 

	if ( ! obj_val.is_object() )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("Top of stack not an object %s at ActionEnum2 "
			" execution",
			obj_val.to_debug_string().c_str());
		);
		return;
	}

	boost::intrusive_ptr<as_object> obj = obj_val.to_object();

	enumerateObject(env, *obj);

}

void
SWFHandlers::ActionBitwiseAnd(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	thread.ensureStack(2); 

	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);

	env.top(1) = int(operand1)&int(operand2); 
	env.drop(1);
}

void
SWFHandlers::ActionBitwiseOr(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;
	thread.ensureStack(2); 

	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);

	env.top(1) = int(operand1)|int(operand2); 
	env.drop(1);
}

void
SWFHandlers::ActionBitwiseXor(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	thread.ensureStack(2); 

	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);

	env.top(1) = int(operand1)^int(operand2); 
	env.drop(1);
}

void
SWFHandlers::ActionShiftLeft(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	// env.top(1).asr(env.top(0));

	as_environment& env = thread.env;
	thread.ensureStack(2); 

	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);

	env.top(1) = int(operand1) >> int(operand2); 
	env.drop(1);
}

void
SWFHandlers::ActionShiftRight(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
//
	//env.top(1).lsr(env.top(0));

	as_environment& env = thread.env;
	thread.ensureStack(2); 

	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);

	env.top(1) = int(operand1) >> int(operand2); 
	env.drop(1);
}

void
SWFHandlers::ActionShiftRight2(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	// env.top(1).lsr(env.top(0));

	as_environment& env = thread.env;
	thread.ensureStack(2); 

	double operand1 = env.top(1).to_number(&env);
	double operand2 = env.top(0).to_number(&env);

	env.top(1) = uint32_t(operand1) >> int(operand2); 
	env.drop(1);
}

void
SWFHandlers::ActionStrictEq(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	as_environment& env = thread.env;
	thread.ensureStack(2); 
	env.top(1).set_bool(env.top(1).strictly_equals(env.top(0)));
        env.drop(1);
}

void
SWFHandlers::ActionGreater(ActionExec& thread)
{
	//GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(2); 

	as_value& operand1 = env.top(1);
	as_value& operand2 = env.top(0);

	if ( operand1.is_string() && operand2.is_string() )
	{
		env.top(1).set_bool(operand1.to_std_string() > operand2.to_std_string());
	}
	else 
	{
		double op1 = operand1.to_number(&env);
		double op2 = operand2.to_number(&env);

		if ( isnan(op1) || isnan(op2) )
		{
			env.top(1).set_undefined();
		}
		else
		{
			env.top(1).set_bool(op1 > op2);
		}
	}
	env.drop(1);
}

void
SWFHandlers::ActionStringGreater(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
    as_environment& env = thread.env;
    thread.ensureStack(2); 
    env.top(1).set_bool(env.top(1).to_tu_string() > env.top(0).to_tu_string());
    env.drop(1);
}

void
SWFHandlers::ActionExtends(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	thread.ensureStack(2);  // super, sub

	as_function* super = env.top(0).to_as_function();
	as_function* sub = env.top(1).to_as_function();

	if ( ! super || ! sub )
	{
		IF_VERBOSE_ASCODING_ERRORS
		(
			if ( ! super )
			{
				log_aserror("ActionExtends: Super is not an as_function (%s)",
					env.top(0).to_debug_string().c_str());
			}
			if ( ! sub )
			{
				log_aserror("ActionExtends: Sub is not an as_function (%s)",
					env.top(1).to_debug_string().c_str());
			}
		);
		env.drop(2);
		return;
	}
	env.drop(2);

	sub->extends(*super);

	//dbglogfile << __PRETTY_FUNCTION__ << ": testing!" << endl;
}

void
SWFHandlers::ActionConstantPool(ActionExec& thread)
{
//    GNASH_REPORT_FUNCTION;
	thread.code.process_decl_dict(thread.pc, thread.next_pc);
}

void
SWFHandlers::ActionDefineFunction2(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	// Code starts at thread.next_pc as the DefineFunction tag
	// contains name and args, while next tag is first tag
	// of the function body.
	swf_function* func = new swf_function(
		&code, &env, thread.next_pc, thread.getWithStack());

	func->set_is_function2();

	size_t i = thread.pc + 3; // skip tag id and length

	// Extract name.
	// @@ security: watch out for possible missing terminator here!
	string name = code.read_string(i);
	i += name.length() + 1; // add NULL-termination

	//cerr << " name:" << name << endl;

	// Get number of arguments.
	unsigned nargs = code.read_int16(i);
	i += 2;

	//cerr << " nargs:" << nargs << endl;

	// Get the count of local registers used by this function.
	uint8 register_count = code[i];
	i++;

	//cerr << " nregisters:" << nargs << endl;

	func->set_local_register_count(register_count);

	// Flags, for controlling register assignment of implicit args.
	uint16	flags = code.read_int16(i);
	i += 2;

	func->set_function2_flags(flags);

	// Get the register assignments and names of the arguments.
	for (unsigned n = 0; n < nargs; n++)
	{
		uint8 arg_register = code[i];
		++i;
	
		// @@ security: watch out for possible missing terminator here!
		const char* arg = code.read_string(i);

		//log_msg("Setting register %d/%d to %s", arg_register, nargs, arg);

		func->add_arg(arg_register, arg);
		i += strlen(arg)+1;
	}

	// Get the length of the actual function code.
	uint16_t code_size = code.read_int16(i);

	// Check code_size value consistency 
	size_t actionbuf_size = thread.code.size();
	if ( thread.next_pc+code_size > actionbuf_size )
	{
		IF_VERBOSE_MALFORMED_SWF(
			log_swferror("function2 code len (%u) "
				"overflows DOACTION tag boundaries "
				"(DOACTION tag len=" SIZET_FMT
				", function2 code offset=" SIZET_FMT "). "
				"Forcing code len to eat the whole buffer "
				"(would this work?).",
				code_size, actionbuf_size, thread.next_pc);
		);
		code_size = actionbuf_size-thread.next_pc;
	}

	i += 2;
	func->set_length(code_size);

	// Skip the function body (don't interpret it now).
	thread.next_pc += code_size; 

	// If we have a name, then save the function in this
	// environment under that name.
	as_value function_value(func);
	if (name.length() > 0)
	{
		//env.set_member(name, function_value);
		thread.setVariable(name, function_value);
	}

	// Otherwise push the function literal on the stack
	else
	{
		env.push_val(function_value);
	}
#ifdef USE_DEBUGGER
	// WARNING: function_value.to_object() can return a newly allocated
	//          thing into the intrusive_ptr, so the debugger
	//          will be left with a deleted object !!
	//          Rob: we don't want to use void pointers here..
	boost::intrusive_ptr<as_object> o = function_value.to_object();
	o->add_ref(); // this will leak, but at least debugger won't end up
	              // with a dandling reference...
        debugger.addSymbol(o.get(), name);
#endif
}

void
SWFHandlers::ActionTry(ActionExec& /*thread*/)
{
//    GNASH_REPORT_FUNCTION;
    dbglogfile << __PRETTY_FUNCTION__ << ": unimplemented!" << endl;
}

/// See: http://sswf.sourceforge.net/SWFalexref.html#action_with
void
SWFHandlers::ActionWith(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	const action_buffer& code = thread.code;
	size_t pc = thread.pc;

	assert( code[pc] == SWF::ACTION_WITH );

	thread.ensureStack(1);  // the object
	boost::intrusive_ptr<as_object> with_obj = env.pop().to_object();

	const vector<with_stack_entry>& with_stack = thread.getWithStack();
	IF_VERBOSE_ACTION (
	log_action("-------------- with block start: stack size is " SIZET_FMT,
		   with_stack.size());
	);

	++pc; // skip tag code

	int tag_length = code.read_int16(pc); // read tag len (should be 2)
	if ( tag_length != 2 )
	{
		IF_VERBOSE_MALFORMED_SWF(
		log_swferror("ActionWith tag length != 2 - skipping");
		);
		return;
	}
	pc += 2; // skip tag len

	unsigned block_length = code.read_int16(pc); // read 'with' body size
	if ( block_length == 0 )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror("Empty with() block...");
		);
		return;
	}
	pc += 2; // skip with body size

	// now we should be on the first action of the 'with' body
	assert(thread.next_pc == pc);

	// where does the 'with' block ends ?
	unsigned block_end = thread.next_pc + block_length;

	if ( ! thread.pushWithEntry(with_stack_entry(with_obj, block_end)) )
	{
		// skip the full block
		log_warning("With block skipped"
			" (with stack size exceeds limit of "
			SIZET_FMT " elements)",
			thread.getWithStackLimit());
		thread.next_pc += block_length;
	}

}

void
SWFHandlers::ActionDefineFunction(ActionExec& thread)
{
//        GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;
	const action_buffer& code = thread.code;

	int16_t length = code.read_int16(thread.pc+1);
	assert( length >= 0 );

	//cerr << " length:" << length << endl;

	// Create a new swf_function
	// Code starts at thread.next_pc as the DefineFunction tag
	// contains name and args, while next tag is first tag
	// of the function body.
	swf_function* func = new swf_function(
		&code, &env, thread.next_pc, thread.getWithStack());

	size_t i = thread.pc + 3;

	// Extract name.
	// @@ security: watch out for possible missing terminator here!
	string name = code.read_string(i);
	i += name.length() + 1;

	//cerr << " name:" << name << endl;

	// Get number of arguments.
	unsigned nargs = code.read_int16(i);
	i += 2;

	//cerr << " nargs:" << nargs << endl;

	// Get the names of the arguments.
	for (unsigned n = 0; n < nargs; n++)
	{
		const char* arg = code.read_string(i);
		//cerr << " arg" << n << " : " << arg << endl;

		// @@ security: watch out for possible missing terminator here!
		func->add_arg(0, arg);
		// wouldn't it be simpler to use strlen(arg)+1 ?
		i += strlen(arg)+1; // func->m_args.back().m_name.length() + 1;
	}

	// Get the length of the actual function code.
	int16_t code_size = code.read_int16(i);

	//cerr << " code size:" << code_size << endl;

	func->set_length(code_size);

    
	// Skip the function body (don't interpret it now).
	// next_pc is assumed to point to first action of
	// the function body (one-past the current tag, whic
	// is DefineFunction). We add code_size to it.
	thread.next_pc += code_size;

	// If we have a name, then save the function in this
	// environment under that name.
	as_value	function_value(func);
	if (name.length() > 0)
	{
		//env.set_member(name, function_value);
		thread.setVariable(name, function_value);
#ifdef USE_DEBUGGER
		// WARNING: new_obj.to_object() can return a newly allocated
		//          thing into the intrusive_ptr, so the debugger
		//          will be left with a deleted object !!
		//          Rob: we don't want to use void pointers here..
		boost::intrusive_ptr<as_object> o = function_value.to_object();
		o->add_ref(); // this will leak, but at least debugger won't end up
	              // with a dandling reference...
                debugger.addSymbol(o.get(), name);
#endif
	}

	// Otherwise push the function literal on the stack
	else
	{
		env.push_val(function_value);
	}

	//cerr << "After ActionDefineFunction:"<<endl;
	//env.dump_stack();
}

void
SWFHandlers::ActionSetRegister(ActionExec& thread)
{
//	GNASH_REPORT_FUNCTION;

	as_environment& env = thread.env;

	thread.ensureStack(1); 

	const action_buffer& code = thread.code;

	uint8_t reg = code[thread.pc + 3];

	// Save top of stack in specified register.
	if ( thread.isFunction2() && reg < env.num_local_registers() )
	{
		env.local_register(reg) = env.top(0);
	      
		IF_VERBOSE_ACTION (
		log_action("-------------- local register[%d] = '%s'",
			reg, env.top(0).to_debug_string().c_str());
		);
	}
	else if (reg < 4)
	{
		env.global_register(reg) = env.top(0);
		      
		IF_VERBOSE_ACTION (
		log_action("-------------- global register[%d] = '%s'",
			reg, env.top(0).to_debug_string().c_str() );
		);

	}
	else
	{
		IF_VERBOSE_MALFORMED_SWF(
		log_swferror("store_register[%d] -- register out of bounds!", reg);
		);
	}
		  
}

const char*
SWFHandlers::action_name(action_type x) const
{
	if ( static_cast<size_t>(x) > get_handlers().size() )
	{
		log_error("at SWFHandlers::action_name(%d) call time, _handlers size is " 
		          SIZET_FMT, x, get_handlers().size());
		return NULL;
	}
	else
	{
		return get_handlers()[x].getName().c_str();
	}
}

} // namespace gnash::SWF

} // namespace gnash
