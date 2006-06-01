//  
//  For more information, please see: http://software.sci.utah.edu
//  
//  The MIT License
//  
//  Copyright (c) 2004 Scientific Computing and Imaging Institute,
//  University of Utah.
//  
//  License for the specific language governing rights and limitations under
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included
//  in all copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  
//    File   : ToolManager.cc
//    Author : Martin Cole
//    Date   : Thu May 25 18:52:10 2006

#include <Core/Events/Tools/ToolManager.h>
#include <Core/Events/Tools/BaseTool.h>
#include <Core/Events/BaseEvent.h>

namespace SCIRun {


ToolManager::ToolManager(string name) :
  name_(name)
{
}

ToolManager::~ToolManager()
{
}


void 
ToolManager::add_tool(tool_handle_t t, unsigned priority) 
{
  //! get the stack for the priority, adding it if it does not yet exist.
  ts_stack_t &s  = stacks_[priority];
  s.push(t);
  //! also store off the tool by name.
  pair<tool_handle_t, unsigned> tp(t, priority);
  tools_[t->get_name()] = tp;
}

BaseTool::return_state_e
ToolManager::send_pointer_event(PointerTool *pt, event_handle_t event) const 
{
  PointerEvent *pe = dynamic_cast<PointerEvent *>(event.get_rep());
  ASSERT(pe);

  int which = 0;
  unsigned int s = pe->get_pointer_state();

  if (s & PointerEvent::BUTTON_1_E) which = 1;
  if (s & PointerEvent::BUTTON_2_E) which = 2;
  if (s & PointerEvent::BUTTON_3_E) which = 3;
  if (s & PointerEvent::BUTTON_4_E) which = 4;
  if (s & PointerEvent::BUTTON_5_E) which = 5;

  BaseTool::return_state_e rstate = BaseTool::CONTINUE_UNCHANGED_E;
  if (s & PointerEvent::MOTION_E) {
    rstate = pt->pointer_motion(which, pe->get_x(), 
				pe->get_y(), pe->get_time());
    
  } else if (s & PointerEvent::BUTTON_PRESS_E) {
    rstate = pt->pointer_down(which, pe->get_x(), 
			      pe->get_y(), pe->get_time());

  } else if (s & PointerEvent::BUTTON_RELEASE_E) {
    rstate = pt->pointer_up(which, pe->get_x(), 
			    pe->get_y(), pe->get_time());
  }

  return rstate;
}

BaseTool::return_state_e
ToolManager::send_key_event(KeyTool *kt, event_handle_t event) const
{
  KeyEvent *ke = dynamic_cast<KeyEvent *>(event.get_rep());
  ASSERT(ke); 

  BaseTool::return_state_e rstate = BaseTool::CONTINUE_UNCHANGED_E;
  unsigned int s = ke->get_key_state();

  if (s & KeyEvent::KEY_PRESS_E) {
    rstate = kt->key_press(ke->get_key_string(), ke->get_keyval(), 
			   ke->get_modifiers(), ke->get_time());

  } else if (s & KeyEvent::KEY_RELEASE_E) {
    rstate = kt->key_release(ke->get_key_string(), ke->get_keyval(), 
			     ke->get_modifiers(), ke->get_time());
  } else {
    ASSERTFAIL("key event not a press or a release?!");
  }

  return rstate;
}

BaseTool::return_state_e
ToolManager::send_window_event(WindowTool *wt, event_handle_t event) const
{
  WindowEvent *we = dynamic_cast<WindowEvent *>(event.get_rep());
  ASSERT(we); 

  BaseTool::return_state_e rstate = BaseTool::CONTINUE_UNCHANGED_E;
  unsigned int s = we->get_window_state();

  if (s & WindowEvent::CREATE_E) {
    rstate = wt->create_notify(we->get_time());

  } else if (s & WindowEvent::DESTROY_E) {
    rstate = wt->destroy_notify(we->get_time());

  } else if (s & WindowEvent::ENTER_E) {
    rstate = wt->enter_notify(we->get_time());

  } else if (s & WindowEvent::LEAVE_E) {
    rstate = wt->leave_notify(we->get_time());

  } else if (s & WindowEvent::EXPOSE_E) {
    rstate = wt->expose_notify(we->get_time());

  } else if (s & WindowEvent::CONFIGURE_E) {
    rstate = wt->configure_notify(we->get_time());

  } else if (s & WindowEvent::REDRAW_E) {
    rstate = wt->redraw_notify(we->get_time());

  } else {
    ASSERTFAIL("key event not a press or a release?!");
  }

  return rstate;
}


event_handle_t 
ToolManager::propagate_event(event_handle_t event)
{

  ps_map_t::iterator iter = stacks_.begin();
  while (iter != stacks_.end()) {
    ts_stack_t &s = (*iter).second; ++iter;
    tool_handle_t t = s.top();

    BaseTool::return_state_e rstate = BaseTool::CONTINUE_UNCHANGED_E;

    if (event->is_pointer_event()) {
      PointerTool* pt = dynamic_cast<PointerTool*>(t.get_rep());
      if (pt) {
	cerr << "propagating pointer event to tool: " << t->get_name() << endl;
	rstate = send_pointer_event(pt, event);
      }
    } else if (event->is_key_event()) {
      KeyTool* kt = dynamic_cast<KeyTool*>(t.get_rep());
      if (kt) {
	cerr << "propagating key event to tool: " << t->get_name() << endl;
	rstate = send_key_event(kt, event);
      }
    } else if (event->is_window_event()) {
      WindowTool* wt = dynamic_cast<WindowTool*>(t.get_rep());
      if (wt) {
	cerr << "propagating window event to tool: " << t->get_name() << endl;
	rstate = send_window_event(wt, event);
      }
    } else {
      cerr << "propagating unknown event to tool: " << t->get_name() << endl;
      rstate = t->process_event(event);
    }
    // if NULL return_event, then the event was consumed.
    if (rstate == BaseTool::STOP_PROPAGATION_E) break;
    if (rstate == BaseTool::MODIFIED_PENDING_E) {
      t->get_modified_event(event); //get the modified event from the tool.
    }
  }
  return event;
}

} // namespace SCIRun
