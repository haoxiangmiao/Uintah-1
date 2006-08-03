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

#include <Dataflow/Network/Module.h>
#include <Core/Malloc/Allocator.h>

#include <Core/Bundle/Bundle.h>
#include <Dataflow/Network/Ports/BundlePort.h>

namespace ModelCreation {

using namespace SCIRun;

class BundleBreakPoint : public Module {
public:
  BundleBreakPoint(GuiContext*);
  virtual void execute();
  
private:
  GuiInt play_stop_;
  BundleHandle buffer_; // input buffer
  BundleHandle cache_; // output buffer
};


DECLARE_MAKER(BundleBreakPoint)
BundleBreakPoint::BundleBreakPoint(GuiContext* ctx)
  : Module("BundleBreakPoint", ctx, Source, "BreakPoint", "ModelCreation"),
    play_stop_(ctx->subVar("play-stop"))
{
}

void BundleBreakPoint::execute()
{
  // Get data from the input port:
  BundleHandle bundle = 0;
  get_input_handle("Bundle",bundle,false);
  
  // If we have new data buffer/cache it:
  if (bundle.get_rep()) buffer_ = bundle;
  
  // If we are not blocking copy data to output cache:
  if (play_stop_.get()) cache_ = buffer_;

  // Copy output so it will not get dereferenced:
  BundleHandle output = cache_;  
  send_output_handle("Bundle",output,false);
}

} // End namespace ModelCreation


