// include before other files so gcc 3.4 can properly instantiate template
#include <Packages/Uintah/Core/Math/Matrix3.h>
#include <Packages/Uintah/Core/Variables/SFCZVariable.h>
#include <Core/Geometry/Vector.h>
#include <Packages/Uintah/Core/Disclosure/TypeUtils.h>
#include <utility>
using namespace Uintah;
using std::pair;

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma set woff 1468
#endif

template class SFCZVariable<SCIRun::Vector>;
template class SFCZVariable<Uintah::Matrix3>;
template class SFCZVariable<double>;
template class SFCZVariable<float>;
template class SFCZVariable<int>;
template class SFCZVariable<long64>;

#if defined(__sgi) && !defined(__GNUC__) && (_MIPS_SIM != _MIPS_SIM_ABI32)
#pragma reset woff 1468
#endif
