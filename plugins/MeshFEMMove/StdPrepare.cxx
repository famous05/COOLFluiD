#include "MeshFEMMove/MeshFEMMove.hh"
#include "StdPrepare.hh"
#include "Framework/MeshData.hh"
#include "Framework/PhysicalModel.hh"
#include "Framework/MethodCommandProvider.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace COOLFluiD::Framework;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace MeshFEMMove {

//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider<StdPrepare, FEMMoveData, MeshFEMMoveModule> stdPrepareProvider("StdPrepare");

//////////////////////////////////////////////////////////////////////////////

void StdPrepare::execute()
{

 }

//////////////////////////////////////////////////////////////////////////////

    } // namespace MeshFEMMove

  } // namespace Numerics

} // namespace COOLFluiD