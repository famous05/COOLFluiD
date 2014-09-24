#include "GReKOReactionTerm.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Physics {

    namespace GReKO {

//////////////////////////////////////////////////////////////////////////////

void GReKOReactionTerm::defineConfigOptions(Config::OptionList& options)
{
//   options.addConfigOption< std::string >("Model","Type of kOmega model to use (kOmega/BSL/SST).");
}

//////////////////////////////////////////////////////////////////////////////

GReKOReactionTerm::GReKOReactionTerm(const std::string& name) :
  BaseTerm(name)
{
   addConfigOptionsTo(this);

//    _modelStr = "kOmega";
//    setParameter("Model",&_modelStr);

}

//////////////////////////////////////////////////////////////////////////////

GReKOReactionTerm::~GReKOReactionTerm()
{
}

//////////////////////////////////////////////////////////////////////////////

void GReKOReactionTerm::configure ( Config::ConfigArgs& args )
{
  BaseTerm::configure(args);

}

//////////////////////////////////////////////////////////////////////////////

void GReKOReactionTerm::resizePhysicalData(RealVector& physicalData)
{
  // resize the physical data
  cf_assert(getDataSize() > 0);
  physicalData.resize(getDataSize());
}

//////////////////////////////////////////////////////////////////////////////

void GReKOReactionTerm::setupPhysicalData()
{
  cf_assert(getDataSize() > 0);

  // set the size of each physical data in the StatesData
  
  resizePhysicalData(m_physicalData);
  resizePhysicalData(m_refPhysicalData);
}

//////////////////////////////////////////////////////////////////////////////

    } // namespace GReKO

  } // namespace Physics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////