/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/


//#include <wx/region.h>
//#include <wx/dc.h>

#include <wx/dcbuffer.h>
#include <wx/gdicmn.h> // color database
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/gbsizer.h>
#include <wx/gauge.h>

#include <CCA/Components/Builder/ComponentIcon.h>
#include <CCA/Components/Builder/PortIcon.h>
#include <CCA/Components/Builder/NetworkCanvas.h>

#include <string>

namespace GUIBuilder {

using namespace SCIRun;

BEGIN_EVENT_TABLE(ComponentIcon, wxPanel)
  //EVT_PAINT(ComponentIcon::OnPaint)
  //EVT_ERASE_BACKGROUND(ComponentIcon::OnEraseBackground)
  EVT_LEFT_DOWN(ComponentIcon::OnLeftDown)
  EVT_LEFT_UP(ComponentIcon::OnLeftUp)
  //EVT_RIGHT_DOWN(ComponentIcon::OnMouseDown)
  //EVT_MIDDLE_DOWN(ComponentIcon::OnMouseDown)
  EVT_MOTION(ComponentIcon::OnMouseMove)
END_EVENT_TABLE()

IMPLEMENT_DYNAMIC_CLASS(ComponentIcon, wxPanel)

ComponentIcon::ComponentIcon(const sci::cca::BuilderComponent::pointer& bc, wxWindowID winid, NetworkCanvas* parent, const sci::cca::ComponentID::pointer& compID, int x, int y)
  : /* dragMode(TEST_DRAG_NONE), */ canvas(parent), hasUIPort(false), hasGoPort(false), isSciPort(false), isMoving(false), cid(compID), builder(bc), ID_MENU_POPUP(BuilderWindow::GetNextID()), ID_BUTTON_UI(BuilderWindow::GetNextID()), ID_BUTTON_STATUS(BuilderWindow::GetNextID()), ID_PROGRESS(BuilderWindow::GetNextID())
{

  Init();
  Create(parent, winid, wxPoint(x, y));
  //Show(true);
}

ComponentIcon::~ComponentIcon()
{
}

bool ComponentIcon::Create(wxWindow* parent, wxWindowID winid, const wxPoint& pos, const wxSize& size, long style)
{
  if (! wxPanel::Create(parent, winid, pos, size, style)) {
    return false;
  }

  SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "", wxFONTENCODING_SYSTEM));

// test
//SetOwnBackgroundColour(wxTheColourDatabase->Find("GOLDENROD"));

  const int GAP_SIZE = 1;
  const int BORDER_SIZE = 4;
  const int PORT_BORDER_SIZE = 10;
  gridBagSizer = new wxGridBagSizer(GAP_SIZE, GAP_SIZE);

  gridBagSizer->AddGrowableCol(0);
  gridBagSizer->AddGrowableCol(5);

  SSIDL::array1<std::string> providedPorts;
  builder->getProvidedPortNames(cid, providedPorts);

  if (providedPorts.size() == 0) {
    gridBagSizer->Add(PortIcon::PORT_WIDTH, PortIcon::PORT_HEIGHT, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, PORT_BORDER_SIZE);
  } else {
    for (unsigned int i = 0; i < providedPorts.size(); i++) {
      if (providedPorts[i] == "ui") {
	hasUIPort = true;
      } else if (providedPorts[i] == "sci.ui") {
	hasUIPort = true;
	isSciPort = true;
      } else if (providedPorts[i] == "go") {
	hasGoPort = true;
      } else if (providedPorts[i] == "sci.go") {
	hasGoPort = true;
	isSciPort = true;
      } else {
	PortIcon *pi = new PortIcon(this, wxID_ANY, Builder::Provides, providedPorts[i]);
	ports[providedPorts[i]] = pi;
	gridBagSizer->Add(pi, wxGBPosition(i, 0), wxDefaultSpan,
			  wxFIXED_MINSIZE|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, PORT_BORDER_SIZE);
      }
    }
  }

  SSIDL::array1<std::string> usedPorts;
  builder->getUsedPortNames(cid, usedPorts);

  if (usedPorts.size() == 0) {
    gridBagSizer->Add(PortIcon::PORT_WIDTH, PortIcon::PORT_HEIGHT, wxGBPosition(0, 5), wxDefaultSpan, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, PORT_BORDER_SIZE);
  } else {
    for (unsigned int i = 0; i < usedPorts.size(); i++) {
      //int id = BuilderWindow::GetNextID();
      PortIcon *pi = new PortIcon(this, wxID_ANY, Builder::Uses, usedPorts[i]);
      ports[usedPorts[i]] = pi;
      gridBagSizer->Add(pi, wxGBPosition(i, 5), wxDefaultSpan, wxFIXED_MINSIZE|wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, PORT_BORDER_SIZE);
    }
  }

  const wxSize UI_SIZE(30, 30);
  uiButton = new wxButton(this, ID_BUTTON_UI, wxT("UI"), wxDefaultPosition, UI_SIZE, wxDOUBLE_BORDER|wxRAISED_BORDER);
  gridBagSizer->Add(uiButton, wxGBPosition(0, 1), /* wxDefaultSpan */ wxGBSpan(2, 1), wxFIXED_MINSIZE|wxALIGN_CENTER, BORDER_SIZE);
  if (! hasUIPort) {
    uiButton->Enable(false);
  }

  label = new wxStaticText(this, wxID_ANY, cid->getInstanceName(), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
  gridBagSizer->Add(label, wxGBPosition(0, 2), wxGBSpan(1, 2), wxALL|wxALIGN_CENTER, BORDER_SIZE);

  timeLabel = new wxStaticText(this, wxID_ANY, wxT("0.0"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
  gridBagSizer->Add(timeLabel, wxGBPosition(1, 2), wxDefaultSpan, wxALL|wxALIGN_CENTER_HORIZONTAL, BORDER_SIZE);

  const int PROG_LEN = 50;
  const wxSize PROG_SIZE(PROG_LEN, 15);
  progressGauge = new wxGauge(this, ID_PROGRESS, PROG_LEN, wxDefaultPosition, PROG_SIZE,  wxDOUBLE_BORDER|wxSUNKEN_BORDER|wxGA_HORIZONTAL|wxGA_SMOOTH);
  gridBagSizer->Add(progressGauge, wxGBPosition(1, 3), wxDefaultSpan, wxALL|wxALIGN_LEFT, BORDER_SIZE);

  const wxSize STATUS_SIZE(15, 15);
  msgButton = new wxButton(this, ID_BUTTON_STATUS, wxT(""), wxDefaultPosition, STATUS_SIZE, wxDOUBLE_BORDER|wxRAISED_BORDER);
  gridBagSizer->Add(msgButton, wxGBPosition(1, 4), wxDefaultSpan, wxFIXED_MINSIZE|wxALIGN_LEFT, BORDER_SIZE);

  SetSizerAndFit(gridBagSizer);

  SetExtraStyle(wxWS_EX_BLOCK_EVENTS);

  wxSize cs = GetClientSize();
  wxSize s = GetSize();
  borderSize = s - cs;

  return true;
}

void ComponentIcon::OnPaint(wxPaintEvent& event)
{
std::cerr << "ComponentIcon::OnPaint()" << std::endl;

// paint border etc. here

// try using wxWindowDC to paint ports on ComponentIcon?

//   wxBufferedPaintDC dc(this);
//   wxColour backgroundColour = GetBackgroundColour();
//   if (!backgroundColour.Ok())
//     backgroundColour =
//       wxSystemSettings::GetColour(wxSYS_COLOUR_3DFACE);

//   dc.SetBrush(wxBrush(backgroundColour));
//   dc.SetPen(wxPen(backgroundColour, 1));

//   wxRect windowRect(wxPoint(0, 0), GetClientSize());
//   dc.DrawRectangle(windowRect);

//   wxRect test(GetPosition(), borderSize);
//   dc.DrawRectangle(test);

//   wxBrush* b = wxTheBrushList->FindOrCreateBrush(wxSYS_COLOUR_3DHILIGHT, wxSOLID);
//   wxPen* p = wxThePenList->FindOrCreatePen(wxSYS_COLOUR_3DHILIGHT, 1, wxSOLID);
//   wxBrush* b = wxTheBrushList->FindOrCreateBrush("MAGENTA", wxSOLID);
//   wxPen* p = wxThePenList->FindOrCreatePen("MAGENTA", 1, wxSOLID);
//   dc.SetBrush(*b);
//   dc.SetPen(*p);
//   dc.DrawRectangle(0, 0, 4, GetSize().GetHeight());

//   b = wxTheBrushList->FindOrCreateBrush("BLUE", wxSOLID);
//   p = wxThePenList->FindOrCreatePen("BLUE", 1, wxSOLID);
//   dc.SetBrush(*b);
//   dc.SetPen(*p);
//   dc.DrawRectangle(0, 0, GetSize().GetWidth(), 4);

//   b = wxTheBrushList->FindOrCreateBrush("GREEN", wxSOLID);
//   p = wxThePenList->FindOrCreatePen("GREEN", 1, wxSOLID);
//   dc.SetBrush(*b);
//   dc.SetPen(*p);
//   dc.DrawRectangle(GetSize().GetWidth()-4, 0, 4, GetSize().GetHeight());

//   b = wxTheBrushList->FindOrCreateBrush("YELLOW", wxSOLID);
//   p = wxThePenList->FindOrCreatePen("YELLOW", 1, wxSOLID);
//   dc.SetBrush(*b);
//   dc.SetPen(*p);
//   dc.DrawRectangle(0, GetSize().GetHeight()-4, GetSize().GetWidth(), 4);
}

void ComponentIcon::OnLeftDown(wxMouseEvent& event)
{
  wxPoint mp;
  canvas->GetUnscrolledMousePosition(mp);
  canvas->GetUnscrolledPosition(event.GetPosition(), movingStart);

  std::cerr << "ComponentIcon::OnLeftDown(..) pos=(" << movingStart.x << ", " << movingStart.y << ") "
            << "mouse pos=(" << mp.x << ", " << mp.y << ")"
            << std::endl;

  isMoving = true;
}

void ComponentIcon::OnLeftUp(wxMouseEvent& WXUNUSED(event))
{
  isMoving = false;
}

void ComponentIcon::OnMouseMove(wxMouseEvent& event)
{
  if (event.LeftIsDown() && event.Dragging() && isMoving) {
    Show(false);
    CaptureMouse();
    wxPoint p;
    canvas->GetUnscrolledPosition(event.GetPosition(), p);
    wxPoint mp;
    canvas->GetUnscrolledMousePosition(mp);

    wxPoint pp;
    GetCanvasPosition(pp);
    //canvas->FindIconAtPointer(pp);

    int dx = 0, dy = 0;
    int newX = pp.x + p.x - movingStart.x;
    int newY = pp.y + p.y - movingStart.y;
    //int newX = mp.x + p.x - movingStart.x;
    //int newY = mp.y + p.y - movingStart.y;
    wxPoint topLeft;
    canvas->GetUnscrolledPosition(wxPoint(newX, newY), topLeft);

    std::cerr << "ComponentIcon::OnMouseMove(..) event pos=(" << p.x << ", " << p.y << ")"
            << std::endl
	    << "\tmouse canvas pos=(" << mp.x << ", " << mp.y << ")"
            << std::endl
	    << "\ticon pos=(" << pp.x << ", " << pp.y << ")"
           << std::endl
	    << "\ttop left pos=(" << topLeft.x << ", " << topLeft.y << ")"
            << std::endl;

    // adjust for canvas boundaries
    if (topLeft.x < 0) {
      newX -= topLeft.x;
      if (p.x < 0) {
	mp.x -= p.x;
	p.x = 0;
	WarpPointer(mp.x, mp.y); // move mouse pointer
      }
      dx -= 1;
    }

    if (topLeft.y < 0) {
      newY -= topLeft.y;
      if (p.y < 0) {
	mp.y -= p.y;
	p.y = 0;
	WarpPointer(mp.x, mp.y); // move mouse pointer
      }
      dy -= 1;
    }

//     int cw = canvas->GetVirtualSize().GetWidth();
//     int mw = GetSize().GetWidth();

//     if (topLeft.x > cw - mw) {
//       newX -= topLeft.x - (cw - mw);
//       if (p.x > cw) {
//         mp.x -= (p.x - cw);
//         p.x = cw - mw;
// 	WarpPointer(mp.x, mp.y);
//       }
//       dx = 1;
//     }

    int ch = canvas->GetVirtualSize().GetHeight();
    int mh = GetSize().GetHeight();

    if (topLeft.y > ch - mh) {
      newY -= topLeft.y - (ch - mh);
      if (p.y > ch) {
        mp.y -= (p.y - ch);
        p.y = ch;
	WarpPointer(mp.x, mp.y);
      }
      dy = 1;
    }

    movingStart = p;
//     wxPoint np;
//     canvas->GetScrolledPosition(wxPoint(newX, newY), np);
//     std::cerr << "\tmove to scrolled (" << np.x << ", " << np.y << ") or unscrolled (" << newX << ", " << newY << ")" << std::endl;
    //Move(np.x, np.y);
    Move(newX, newY);
    Show(true);
    ReleaseMouse();

    canvas->Refresh();


//     CalcScrolledPosition(newX, newY, &newX, &newY);
//     movingIcon->Move(newX, newY);
//     movingIcon->Show(true);
//     // reset moving icon connections
//     Refresh();
//     builderWindow->RedrawMiniCanvas();

//     wxRect windowRect = GetClientRect();
//     if (! windowRect.Inside(newX + mw, newY + mh)) {
//       int xu = 0, yu = 0;
//       GetScrollPixelsPerUnit(&xu, &yu);
//       Scroll(newX/xu, newY/yu);
//     }
  }



}

void ComponentIcon::GetCanvasPosition(wxPoint& p)
{
  canvas->GetUnscrolledPosition(this->GetPosition(), p);
}


///////////////////////////////////////////////////////////////////////////
// protected constructor and member functions

ComponentIcon::ComponentIcon() : ID_MENU_POPUP(BuilderWindow::GetNextID()), ID_BUTTON_UI(BuilderWindow::GetNextID()), ID_BUTTON_STATUS(BuilderWindow::GetNextID()), ID_PROGRESS(BuilderWindow::GetNextID())
{
  Init();
}

void ComponentIcon::Init()
{
}

}
