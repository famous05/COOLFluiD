// Copyright (C) 2012 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifdef CF_HAVE_MPI
#include "Common/MPI/MPIStructDef.hh"
#endif

#include "Common/PE.hh"

#include "Common/CFLog.hh"
#include "Common/NullPointerException.hh"
#include "Common/EventHandler.hh"
#include "Common/MemFunArg.hh"

#include "Environment/FileHandlerOutput.hh"
#include "Environment/DirPaths.hh"
#include "Environment/ObjectProvider.hh"
#include "Environment/SingleBehaviorFactory.hh"

#include "Framework/StandardSubSystem.hh"
#include "Framework/MeshData.hh"
#include "Framework/CFL.hh"
#include "Framework/PhysicalModel.hh"
#include "Framework/MeshCreator.hh"
#include "Framework/MeshAdapterMethod.hh"
#include "Framework/ErrorEstimatorMethod.hh"
#include "Framework/CouplerMethod.hh"
#include "Framework/ConvergenceMethod.hh"
#include "Framework/SpaceMethod.hh"
#include "Framework/DataProcessingMethod.hh"
#include "Framework/OutputFormatter.hh"
#include "Framework/LinearSystemSolver.hh"
#include "Framework/SubSystemStatus.hh"
#include "Framework/StopConditionController.hh"
#include "Framework/InteractiveParamReader.hh"
#include "Framework/CommandsToTRSMapper.hh"
#include "Framework/PathAppender.hh"
#include "Framework/NamespaceSwitcher.hh"
#include "Framework/Namespace.hh"
#include "Framework/Framework.hh"
#include "Framework/SimulationStatus.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace boost::filesystem;
using namespace COOLFluiD::Common;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Framework {

//////////////////////////////////////////////////////////////////////////////

Environment::ObjectProvider<StandardSubSystem,
               SubSystem,
               FrameworkLib,
               1>
sSubSysProvider("StandardSubSystem");

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::defineConfigOptions(Config::OptionList& options)
{
   options.addConfigOption< std::vector<std::string> >("LinearSystemSolver","Self-reg keys of the linear system solvers.");
   options.addConfigOption< std::vector<std::string> >("MeshAdapterNames","Names of the mesh adapter.");
   options.addConfigOption< std::vector<std::string> >("DataPostProcessing","Self-reg keys of the data postprocessing.");
   options.addConfigOption< std::vector<std::string> >("DataPreProcessingNames","Names of the data preprocessing.");
   options.addConfigOption< std::vector<std::string> >("MeshAdapterMethod","Self-reg keys of the mesh adapter.");
   options.addConfigOption< std::vector<std::string> >("ConvergenceMethod","Self-reg keys of the convergence method.");
   options.addConfigOption< std::vector<std::string> >("DataPostProcessingNames","Names of the data postprocessing.");
   options.addConfigOption< std::vector<std::string> >("ErrorEstimatorNames","Names of the error estimator.");
   options.addConfigOption< std::vector<std::string> >("MeshCreator","Self-reg keys of the mesh creator.");
   options.addConfigOption< std::vector<std::string> >("ErrorEstimatorMethod","Self-reg keys of the error estimator.");
   options.addConfigOption< std::vector<std::string> >("OutputFormat","Self-reg keys of the output format.");
   options.addConfigOption< std::vector<std::string> >("MeshCreatorNames","Names of the mesh creator.");
   options.addConfigOption< std::vector<std::string> >("ConvergenceMethodNames","Names of the convergence method.");
   options.addConfigOption< std::vector<std::string> >("SpaceMethodNames","Names of the space method.");
   options.addConfigOption< std::vector<std::string> >("CouplerMethod","Self-reg keys of the coupler method.");
   options.addConfigOption< std::vector<std::string> >("SpaceMethod","Self-reg keys of the space method.");
   options.addConfigOption< std::vector<std::string> >("LSSNames","Names of the linear system solvers.");
   options.addConfigOption< std::vector<std::string> >("DataPreProcessing","Self-reg keys of the data preprocessing.");
   options.addConfigOption< std::vector<std::string> >("OutputFormatNames","Names of the output format.");
   options.addConfigOption< std::vector<std::string> >("CouplerMethodNames","Names of the coupler method.");
   options.addConfigOption< std::string >("StopCondition","The stop condition to control the iteration procedure.");
   options.addConfigOption< CFuint >("InitialIter","Initial Iteration Number.");
   options.addConfigOption< CFreal >("InitialTime","Initial Physical Time of the SubSystem.");
   options.addConfigOption< int, Config::DynamicOption<> >("StopSimulation","Flag to force an immediate stop of the simulation.");
}

//////////////////////////////////////////////////////////////////////////////

StandardSubSystem::StandardSubSystem(const std::string& name)
  : SubSystem(name),
    m_duration()
{
   addConfigOptionsTo(this);
   registActionListeners();

  m_stopConditionStr = "MaxNumberSteps";
  setParameter("StopCondition",&m_stopConditionStr);

  // MeshCreator-related configuration options
  setParameter("MeshCreator",&m_meshCreator.mKeys);
  setParameter("MeshCreatorNames",&m_meshCreator.mNames);

  // MeshAdapter-related configuration options
  setParameter("MeshAdapterMethod",&m_meshAdapterMethod.mKeys);
  setParameter("MeshAdapterNames",&m_meshAdapterMethod.mNames);

  // CouplerMethod-related configuration options
  setParameter("CouplerMethod",&m_couplerMethod.mKeys);
  setParameter("CouplerMethodNames",&m_couplerMethod.mNames);

  // ErrorEstimator-related configuration options
  setParameter("ErrorEstimatorMethod",&m_errorEstimatorMethod.mKeys);
  setParameter("ErrorEstimatorNames",&m_errorEstimatorMethod.mNames);

  // SpaceMethod-related configuration options
  setParameter("SpaceMethod",&m_spaceMethod.mKeys);
  setParameter("SpaceMethodNames",&m_spaceMethod.mNames);

  // ConvergenceMethod-related configuration options
  setParameter("ConvergenceMethod",&m_convergenceMethod.mKeys);
  setParameter("ConvergenceMethodNames",&m_convergenceMethod.mNames);

  // LinearSystemSolver-related configuration options
  setParameter("LinearSystemSolver",&m_linearSystemSolver.mKeys);
  setParameter("LSSNames",&m_linearSystemSolver.mNames);

  // DataProcessing-related configuration options
  setParameter("DataPreProcessing",&m_dataPreProcessing.mKeys);
  setParameter("DataPreProcessingNames",&m_dataPreProcessing.mNames);

  // DataProcessing-related configuration options
  setParameter("DataPostProcessing",&m_dataPostProcessing.mKeys);
  setParameter("DataPostProcessingNames",&m_dataPostProcessing.mNames);

  // OutputFormatter-related configuration options
  setParameter("OutputFormat",&m_outputFormat.mKeys);
  setParameter("OutputFormatNames",&m_outputFormat.mNames);

  m_initialTime = 0.;
  setParameter("InitialTime",&m_initialTime);

  m_initialIter = 0;
  setParameter("InitialIter",&m_initialIter);

  m_forcedStop = 0;
  setParameter("StopSimulation",&m_forcedStop);
}

//////////////////////////////////////////////////////////////////////////////

StandardSubSystem::~StandardSubSystem()
{
  CFAUTOTRACE;
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::configure ( Config::ConfigArgs& args )
{
  CFAUTOTRACE;

  SubSystem::configure(args);
  
  // set the physical model
  configurePhysicalModel(args);
  
  // builds a stop condition
  Common::SafePtr<StopCondition::PROVIDER> stopCondProv =
    Environment::Factory<StopCondition>::getInstance().getProvider(m_stopConditionStr);
  SelfRegistPtr<StopCondition> stopCondition =
    stopCondProv->create(stopCondProv->getName());
  
  configureNested ( stopCondition.getPtr(), args );
  
  // Give the stopcondition to the StopConditionController
  // StopConditionController::Create makes a new StopConditionController
  // object, adapted to the current PE mode
  m_stopCondControler.reset(StopConditionController::Create(stopCondition));

  // builds MeshCreator
  configureMultiMethod<MeshCreator>(args,m_meshCreator);

  // builds MeshAdapterMethod
  configureMultiMethod<MeshAdapterMethod>(args,m_meshAdapterMethod);
  
  // builds CouplerMethod
  configureCouplerMethod(args,m_couplerMethod);

  // builds SpaceMethod
  configureMultiMethod<SpaceMethod>(args,m_spaceMethod);
  
  // builds ErrorEstimatorMethod
  configureMultiMethod<ErrorEstimatorMethod>(args,m_errorEstimatorMethod);

  // configure multi method
  configureMultiMethod<LinearSystemSolver>(args,m_linearSystemSolver);

  // builds ConvergenceMethod
  configureMultiMethod<ConvergenceMethod>(args,m_convergenceMethod);

  // builds DataPreProcessing
  configureMultiMethod<DataProcessingMethod>(args,m_dataPreProcessing);
  
  // builds DataPostProcessing
  configureMultiMethod<DataProcessingMethod>(args,m_dataPostProcessing);
  
  // builds OutputFormatter
  configureMultiMethod<OutputFormatter>(args,m_outputFormat);
  
  // set collaborators
  setCollaborators();
  
  setParentNamespaceInMethodSockets(); // after this all the namespaces are set
  setEnableNamespaces(true);
  setNonRootMethods();
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::setNonRootMethods()
{
  CFAUTOTRACE;

  std::vector< Common::SafePtr<Method> > all_methods =
    MethodRegistry::getInstance().getAllMethods();

  std::for_each (all_methods.begin(),
                 all_methods.end(),
                 Common::safeptr_mem_fun(&Method::setNonRootMethods));
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::configureCouplerMethod(Config::ConfigArgs& args, MultiMethodTuple<Framework::CouplerMethod>& couplerMethod)
{
  configureMultiMethod<CouplerMethod>(args, m_couplerMethod);

  //Exchange Info between Namespaces
  m_couplerMethod.apply(mem_fun<void,CouplerMethod>
			(&CouplerMethod::setInfoToOtherSubSystem));
  
  m_couplerMethod.apply(mem_fun<void,CouplerMethod>
			(&CouplerMethod::getInfoFromOtherSubSystem));
  
  // !!! continue the configuration !!!
  for ( CFuint i = 0; i < m_couplerMethod.size();  ++i) {
    m_couplerMethod[i]->postConfigure(args);
  }
}
    
//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::allocateAllSockets()
{
  CFAUTOTRACE;

  SubSystem::allocateAllSockets();
  
  // complement here the plugging of sockets and linking methods
}
    
//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::deallocateAllSockets()
{
  CFAUTOTRACE;

  // complement here the unplugging of sockets and unlinking methods

  SubSystem::deallocateAllSockets();
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::buildMeshData()
{
  CFAUTOTRACE;

  // first setup the physical models
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLog(NOTICE,"Setting up all PhysicalModel's\n\n");
  CFLog(NOTICE,"-------------------------------------------------------------\n");

  // loop on all namespaces and setup the models in each of them
  typedef std::vector<Common::SafePtr<Namespace> > NspVec;
  NamespaceSwitcher& nsw = NamespaceSwitcher::getInstance(SubSystemStatusStack::getCurrentName());
  NspVec lst = nsw.getAllNamespaces();
  cf_assert(!lst.empty()); // there should be some models
  
  for(NspVec::iterator nsp = lst.begin(); nsp != lst.end(); ++nsp)
  {
    nsw.pushNamespace((*nsp)->getName());
    PhysicalModelStack::getActive()->getImplementor()->setup();
    nsw.popNamespace();
  }
  
  // allocate all the mesh data
  vector <Common::SafePtr<MeshData> > meshDataVector = 
    MeshDataStack::getInstance().getAllEntries();
  
  for_each (meshDataVector.begin(), meshDataVector.end(), 
	    safeptr_mem_fun(&MeshData::reallocate));
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLog(NOTICE,"Setting up all MeshCreator's\n");
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  
  // finally setup the mesh creators
  Common::for_each_if (m_meshCreator.begin(), m_meshCreator.end(), 
		       mem_fun(&MeshCreator::setMethod),
		       mem_fun(&MeshCreator::isNonRootMethod), false);
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLog(NOTICE,"Building MeshData's\n");
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  
  Common::for_each_if (m_meshCreator.begin(), m_meshCreator.end(), 
		       mem_fun(&MeshCreator::generateMeshData),
		       mem_fun(&MeshCreator::isNonRootMethod), false);
  
  // Process the CFMeshData to do:
  //  - renumbering
  //  - conversion FVM <-> FEM
  Common::for_each_if (m_meshCreator.begin(), m_meshCreator.end(), 
		       mem_fun(&MeshCreator::processMeshData),
		       mem_fun(&MeshCreator::isNonRootMethod), false);
  
  // Use the CFMeshData to build the mesh
  Common::for_each_if (m_meshCreator.begin(), m_meshCreator.end(), 
		       mem_fun(&MeshCreator::buildMeshData),
		       mem_fun(&MeshCreator::isNonRootMethod), false);
  
  
  const int rank = PE::GetPE().GetRank("Default");
  
  for(CFuint iMeshData = 0; iMeshData < meshDataVector.size(); iMeshData++) {
    SafePtr<MeshData> currMeshData = meshDataVector[iMeshData];
    bool isNonRoot = false;
    for(CFuint iMC = 0; iMC < m_meshCreator.size(); iMC++) {
      if(m_meshCreator[iMC]->getNamespace() == currMeshData->getPrimaryNamespace())
        isNonRoot = m_meshCreator[iMC]->isNonRootMethod();
    }
    
    if (!isNonRoot) {
      // AL: does this work if the same mesh is shared by different namespaces??
      if (PE::GetPE().isRankInGroup(rank, currMeshData->getPrimaryNamespace())) {
	CFLogNotice("Building TRS info for MeshData in Namespace " << currMeshData->getPrimaryNamespace() << "\n");
	// === conversion fix ===
	vector<string>& TotalTRSNames = currMeshData->getTotalTRSNames ();
	vector<vector<CFuint> >& TotalTRSInfo = currMeshData->getTotalTRSInfo ();
	
	if (TotalTRSInfo.empty ()) {
	  cf_assert(TotalTRSNames.empty());
	  vector< SafePtr<TopologicalRegionSet> > trsList = currMeshData->getTrsList();
	  
	  // count in advance the number of writable TRS
	  CFuint sizeTRSToWrite = 0;
	  for (CFuint i = 0; i < trsList.size(); ++i) {
	    if (trsList[i]->hasTag("writable")) {
	      sizeTRSToWrite++;
	    }
	  }
	  cf_assert(sizeTRSToWrite < trsList.size());
	  
	  TotalTRSNames.resize(sizeTRSToWrite);
	  TotalTRSInfo.resize(sizeTRSToWrite);
	  
	  CFuint counter = 0;
	  vector< Common::SafePtr<TopologicalRegionSet> >::iterator it;
	  for (it = trsList.begin(); it != trsList.end(); ++it) {
	    if ((*it)->hasTag("writable")) {
	      TotalTRSNames[counter] = (*it)->getName();
	      const CFuint nbTRs = (*it)->getNbTRs();
	      TotalTRSInfo[counter].resize(nbTRs);
	      for (CFuint iTR = 0; iTR < nbTRs; ++iTR) {
		TotalTRSInfo[counter][iTR] = (*(*it))[iTR]->getLocalNbGeoEnts();
		CFLog(VERBOSE, "TRS : " << (*it)->getName()
		      << ", TR : "<< iTR
		      << ", nbGeos : "
		      << TotalTRSInfo[counter][iTR] << "\n");
	      }
	      counter++;
	    }
	  }
	  
	  cf_assert(counter == sizeTRSToWrite);
	}
	
	if (PE::GetPE().IsParallel()) {
	  const std::string parStateVecName = currMeshData->getPrimaryNamespace() + "_states";
	  const std::string parNodeVecName = currMeshData->getPrimaryNamespace() + "_nodes";
	  
	  DataHandle<State*, GLOBAL> states =
	    currMeshData->getDataStorage()->getGlobalData<State*>(parStateVecName);
	  
	  DataHandle<Node*, GLOBAL> nodes =
	    currMeshData->getDataStorage()->getGlobalData<Node*>(parNodeVecName);
	  
	  // AL : this should be activated only in very special occasions
	  // #ifndef NDEBUG
	  //  states.DumpContents ();
	  //  nodes.DumpContents ();
	  //  #endif
	  
	  states.buildMap ();
	  nodes.buildMap ();
	  
	  // #ifndef NDEBUG
	  //  states.DumpInfo ();
	  //  nodes.DumpInfo ();
	  // #endif
	}
      }
    }
  } 
}
    
//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::setup()
{
  CFAUTOTRACE;

  cf_assert(isConfigured());
  
  //setCommands() needs Trs's => must be exactly here
  setCommands();
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up DataPreProcessing's\n");
  m_dataPreProcessing.apply
    (mem_fun<void,DataProcessingMethod>(&DataProcessingMethod::setMethod));
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up MeshAdapterMethod's\n");
  m_meshAdapterMethod.apply
    (mem_fun<void,MeshAdapterMethod>(&MeshAdapterMethod::setMethod));

  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up ConvergenceMethod's\n");
  m_convergenceMethod.apply
    (mem_fun<void,ConvergenceMethod>(&ConvergenceMethod::setMethod));
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up LinearSystemSolver's\n");
  m_linearSystemSolver.apply
    (mem_fun<void,LinearSystemSolver>(&LinearSystemSolver::setMethod));
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up SpaceMethod's\n");
  m_spaceMethod.apply
    (mem_fun<void,SpaceMethod>(&SpaceMethod::setMethod));
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up ErrorEstimatorMethod's\n");
  m_errorEstimatorMethod.apply
    (mem_fun<void,ErrorEstimatorMethod>(&ErrorEstimatorMethod::setMethod));
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up CouplerMethod's\n");
  setCouplerMethod();

  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up DataPostProcessing's\n");
  m_dataPostProcessing.apply
    (mem_fun<void,DataProcessingMethod>(&DataProcessingMethod::setMethod));

  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("Setting up OutputFormatter's\n");
  m_outputFormat.apply
    (root_mem_fun<void,OutputFormatter>(&OutputFormatter::setMethod));

  vector <Common::SafePtr<SubSystemStatus> > subSysStatusVec =
    SubSystemStatusStack::getInstance().getAllEntries();

  for(CFuint i = 0; i<subSysStatusVec.size();i++){
    // reset to 0 (or Initial Value) the number of iterations and the residual
    subSysStatusVec[i]->setNbIter(m_initialIter);
    subSysStatusVec[i]->setCurrentTimeDim(m_initialTime);
    // reset to 0 the number of iterations and the residual
    subSysStatusVec[i]->resetResidual();
  }
  
  NamespaceSwitcher& nsw = NamespaceSwitcher::getInstance(SubSystemStatusStack::getCurrentName());
  const string sssName = nsw.getName(mem_fun<string,Namespace>(&Namespace::getSubSystemStatusName));
  SafePtr<SubSystemStatus> currSSS = SubSystemStatusStack::getInstance().getEntry(sssName);
  cf_assert(currSSS.isNotNull());
  
  currSSS->setSetup(true);
  
  CFLog(VERBOSE, "StandardSubSystem::setup() => m_dataPreProcessing.apply()\n");
  // pre-process the data
  m_dataPreProcessing.apply(mem_fun<void,DataProcessingMethod>
			    (&DataProcessingMethod::processData));
  
  // initialization has to be done after the creation of the normals
  // => after  m_spaceMethod->setMethod(), because some init
  // commands (init on the wall) need normals (init on the wall)
  CFLog(NOTICE,"\n-------------------------------------------------------------\n");
  CFLogInfo("Initializing solution\n");
  
  m_spaceMethod.apply(mem_fun<void,SpaceMethod>(&SpaceMethod::initializeSolution));
  
  CFLogInfo("Writing initial solution ... \n");
  writeSolution(true);
  
  currSSS->setSetup(false);
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::setCouplerMethod()
{
  m_couplerMethod.apply
      (mem_fun<void,CouplerMethod>(&CouplerMethod::setMethod));

  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("CouplerMethod's write interface coordinates\n");
  m_couplerMethod.apply
      (mem_fun<void,CouplerMethod>(&CouplerMethod::preProcessWrite));

  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("CouplerMethod's read interface coordinates\n");
  m_couplerMethod.apply
      (mem_fun<void,CouplerMethod>(&CouplerMethod::preProcessRead));

  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("CouplerMethod's match mesh and write\n");
  m_couplerMethod.apply
      (mem_fun<void,CouplerMethod>(&CouplerMethod::meshMatchingWrite));

  CFLog(NOTICE,"-------------------------------------------------------------\n");
  CFLogInfo("CouplerMethod's read match mesh\n");
  m_couplerMethod.apply
      (mem_fun<void,CouplerMethod>(&CouplerMethod::meshMatchingRead));
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::run()
{
  CFAUTOTRACE;

  cf_assert(isConfigured());
  m_forcedStop = (int)false;
    
  std::vector <Common::SafePtr<SubSystemStatus> > subSysStatusVec =  
    SubSystemStatusStack::getInstance().getAllEntries();
  
  ///@todo this should not be here!!
  for (CFuint i = 0; i < subSysStatusVec.size(); i++) {
    // reset to 0 (or Initial Value) the number of iterations and the residual
    subSysStatusVec[i]->setNbIter(m_initialIter);
    subSysStatusVec[i]->setCurrentTimeDim(m_initialTime);
    // reset to 0 the number of iterations and the residual
    subSysStatusVec[i]->resetResidual();
    subSysStatusVec[i]->startWatch();
  }
  
  NamespaceSwitcher& nsw = NamespaceSwitcher::getInstance(SubSystemStatusStack::getCurrentName());
  const string sssName = nsw.getName(mem_fun<string,Namespace>(&Namespace::getSubSystemStatusName));
  SafePtr<SubSystemStatus> currSSS = SubSystemStatusStack::getInstance().getEntry(sssName);
  cf_assert(currSSS.isNotNull());
  
  m_duration.set(0.);
  Stopwatch<WallTime> stopTimer;
  stopTimer.start();

  // Transfer of the data for the subsystems coupling
  // here, write the data to the other subsystems
  m_couplerMethod.apply(mem_fun<void,CouplerMethod>(&CouplerMethod::dataTransferWrite));

  for ( ; (!m_stopCondControler->isAchieved(currSSS->getConvergenceStatus())) && (!m_forcedStop); ) {
    
    // read the interactive parameters
    getInteractiveParamReader()->readFile();
    
    CFLog(VERBOSE, "StandardSubSystem::run() => m_dataPreProcessing.apply()\n");
    // pre-process the data
    m_dataPreProcessing.apply(mem_fun<void,DataProcessingMethod>
                              (&DataProcessingMethod::processData));
    
    CFLog(VERBOSE, "StandardSubSystem::run() => m_couplerMethod.apply()\n");
    // Transfer of the data for the subsystems coupling
    // reads the data from other subsystems
    m_couplerMethod.apply(mem_fun<void,CouplerMethod>
                          (&CouplerMethod::dataTransferRead));
    
    CFLog(VERBOSE, "StandardSubSystem::run() => m_meshAdapterMethod.apply()\n");
    // Mesh adaptation (connectivity is preserved)
    m_meshAdapterMethod.apply(mem_fun<void,MeshAdapterMethod>
                              (&MeshAdapterMethod::adaptMesh));
    
    CFLog(VERBOSE, "StandardSubSystem::run() => m_convergenceMethod.apply()\n");
    // Convergence method (iterative process)
    m_convergenceMethod.apply(root_mem_fun<void,ConvergenceMethod>
                              (&ConvergenceMethod::takeStep));
    
    CFLog(VERBOSE, "StandardSubSystem::run() => m_errorEstimatorMethod.apply()\n");
    // estimate errors
    m_errorEstimatorMethod.apply(mem_fun<void,ErrorEstimatorMethod>
                                  (&ErrorEstimatorMethod::estimate));

    CFLog(VERBOSE, "StandardSubSystem::run() => m_dataPostProcessing.apply()\n");
    // post-process the data
    m_dataPostProcessing.apply(mem_fun<void,DataProcessingMethod>
                          (&DataProcessingMethod::processData));
    
    CFLog(VERBOSE, "StandardSubSystem::run() => m_couplerMethod.apply()\n");
    // Transfer of the data for the subsystems coupling
    // here, write the data to the other subsystems
    m_couplerMethod.apply(mem_fun<void,CouplerMethod>
                          (&CouplerMethod::dataTransferWrite));
    
    CFLog(VERBOSE, "StandardSubSystem::run() => m_meshAdapterMethod.apply()\n");
    // Mesh adaptation (connectivity is NOT preserved)
    m_meshAdapterMethod.apply(mem_fun<void,MeshAdapterMethod>
                              (&MeshAdapterMethod::remesh));
    
    CFLog(VERBOSE, "StandardSubSystem::run() => writeConvergenceOnScreen()\n");
    writeConvergenceOnScreen();
    
    CFLog(VERBOSE, "StandardSubSystem::run() => writeSolution()\n");
    // write solution to file
    bool dontforce = false;
    writeSolution(dontforce);

  } // end for convergence loop
 
  CFLog(VERBOSE, "StandardSubSystem::run() => m_errorEstimatorMethod.apply()\n");
  // estimate the error one final time
  m_errorEstimatorMethod.apply(mem_fun<void,ErrorEstimatorMethod>(&ErrorEstimatorMethod::estimate));
  
  std::for_each (subSysStatusVec.begin(), subSysStatusVec.end(),
		 safeptr_mem_fun(&SubSystemStatus::stopWatch));
  
  stopTimer.stop();
  
  CFLog(NOTICE, "SubSystem WallTime: " << stopTimer << "s\n");
  m_duration = subSysStatusVec[0]->readWatchHMS();
 
  // dumpStates();
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::unsetup()
{
  CFAUTOTRACE;
  NamespaceSwitcher& nsw = NamespaceSwitcher::getInstance(SubSystemStatusStack::getCurrentName());
  const string sssName = nsw.getName(mem_fun<string,Namespace>(&Namespace::getSubSystemStatusName));
  SafePtr<SubSystemStatus> subSysStatus = SubSystemStatusStack::getInstance().getEntry(sssName);
  cf_assert(subSysStatus.isNotNull());
  
  vector <Common::SafePtr<SubSystemStatus> > subSysStatusVec =
    SubSystemStatusStack::getInstance().getAllEntries();
  
  CFreal totalResidual = 0.;
  for(CFuint i=0; i<subSysStatusVec.size(); ++i) {
    totalResidual += subSysStatusVec[i]->getResidual();
  }
  
  CFLog(INFO, "Total Number Iter: " << subSysStatus->getNbIter()
        << " Reached Residual: " << totalResidual
        << " and took: " << m_duration.str() << "\n");
 
#ifdef CF_HAVE_MPI
  // AL: the following needs to be generalized for regression testing
  // for now, only rank 0 writes the residual file even in concurrent simulations 
  MPI_Bcast(&totalResidual, 1, MPIStructDef::getMPIType(&totalResidual), 0, PE::GetPE().GetCommunicator("Default")); 
#endif
  
  cf_assert(std::abs(totalResidual) > 0.);
  SimulationStatus::getInstance().getLastResidual() = totalResidual;
  
  bool force = true;
  writeSolution(force);
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => OutputFormatter\n");
  // unset all the methods
  m_outputFormat.apply
    (root_mem_fun<void,OutputFormatter>(&OutputFormatter::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => ErrorEstimatorMethod\n");
  m_errorEstimatorMethod.apply
    (mem_fun<void,ErrorEstimatorMethod>(&ErrorEstimatorMethod::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => SpaceMethod\n");
  m_spaceMethod.apply
    (mem_fun<void,SpaceMethod>(&SpaceMethod::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => LinearSystemSolver\n");
  m_linearSystemSolver.apply
    (mem_fun<void,LinearSystemSolver>(&LinearSystemSolver::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => ConvergenceMethod\n");
  m_convergenceMethod.apply
    (mem_fun<void,ConvergenceMethod>(&ConvergenceMethod::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => MeshAdapterMethod\n");
  m_meshAdapterMethod.apply
    (mem_fun<void,MeshAdapterMethod>(&MeshAdapterMethod::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => CouplerMethod\n");
  m_couplerMethod.apply
    (mem_fun<void,CouplerMethod>(&CouplerMethod::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => MeshCreator\n");
  m_meshCreator.apply
    (root_mem_fun<void,MeshCreator>(&MeshCreator::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => DataProcessingMethod (preprocessing)\n");
  m_dataPreProcessing.apply
    (mem_fun<void,DataProcessingMethod>(&DataProcessingMethod::unsetMethod));
  
  CFLog(VERBOSE, "StandardSubSystem::unsetup() => DataProcessingMethod (postprocessing)\n");
  m_dataPostProcessing.apply
    (mem_fun<void,DataProcessingMethod>(&DataProcessingMethod::unsetMethod));
  
  CFLog(NOTICE,"-------------------------------------------------------------\n");
}

//////////////////////////////////////////////////////////////////////////////

vector<Method*> StandardSubSystem::getMethodList()
{
  vector<Method*>  mList;
  
  copy(m_meshCreator.begin(), m_meshCreator.end(), back_inserter(mList));
  copy(m_couplerMethod.begin(), m_couplerMethod.end(), back_inserter(mList));
  copy(m_meshAdapterMethod.begin(), m_meshAdapterMethod.end(), back_inserter(mList));
  copy(m_errorEstimatorMethod.begin(), m_errorEstimatorMethod.end(), back_inserter(mList));
  copy(m_linearSystemSolver.begin(), m_linearSystemSolver.end(), back_inserter(mList));
  copy(m_convergenceMethod.begin(), m_convergenceMethod.end(), back_inserter(mList));
  copy(m_spaceMethod.begin(), m_spaceMethod.end(), back_inserter(mList));
  copy(m_dataPreProcessing.begin(), m_dataPreProcessing.end(), back_inserter(mList));
  copy(m_dataPostProcessing.begin(), m_dataPostProcessing.end(), back_inserter(mList));
  copy(m_outputFormat.begin(), m_outputFormat.end(), back_inserter(mList));
  
  return mList;
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::registActionListeners()
{
  CFAUTOTRACE;

  // always call the parent class
  SubSystem::registActionListeners();

  // add here other ActionListeners
  Common::SafePtr<EventHandler> event_handler = Environment::CFEnv::getInstance().getEventHandler();
  
  const std::string ssname = SubSystemStatusStack::getCurrentName();
  event_handler->addListener(event_handler->key(ssname, "CF_ON_MAESTRO_MODIFYRESTART"),
			     this,&StandardSubSystem::modifyRestartAction);
  event_handler->addListener(event_handler->key(ssname, "CF_ON_MESHADAPTER_AFTERGLOBALREMESHING"),
			     this,&StandardSubSystem::afterRemeshingAction);
}
    
//////////////////////////////////////////////////////////////////////////////

Common::Signal::return_t StandardSubSystem::afterRemeshingAction(Common::Signal::arg_t eAfterRemesh)
{
  CFAUTOTRACE;

  m_forcedStop = (int)true;
  SimulationStatus::getInstance().setRestart(true);

  return Common::Signal::return_t ();
}

//////////////////////////////////////////////////////////////////////////////

Common::Signal::return_t StandardSubSystem::modifyRestartAction(Common::Signal::arg_t eModifyRestart)
{
  CFAUTOTRACE;
  using namespace COOLFluiD::Environment;
  for(CFuint iMC=0; iMC < m_meshCreator.size(); iMC++)
  {
    if(!m_meshCreator[iMC]->isNonRootMethod())
    {
      std::string subsystemName = getName();
      std::string nspName = m_meshCreator[iMC]->getNamespace();
      std::string filename = SimulationStatus::getInstance().getLastOutputFile(subsystemName, nspName).string();
      
      CFLog(NOTICE,"Restarting from file : " << filename << "\n");
      
      DirPaths::getInstance().setWorkingDir(DirPaths::getInstance().getResultsDir().string());
      m_meshCreator[iMC]->modifyFileNameForRestart(filename);
    }
  }
  
  m_initialTime = SimulationStatus::getInstance().getSimulationTime(getName());
  vector< SafePtr<SubSystemStatus> > subSystemStatusVec =
    SubSystemStatusStack::getInstance().getAllEntries();
  
  for(CFuint i=0; i<subSystemStatusVec.size(); ++i)
  {
    subSystemStatusVec[i]->setCurrentTimeDim(m_initialTime);
  }
  
  return Common::Signal::return_t ();
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::setCollaborators()
{
  CFAUTOTRACE;

  CFLog(VERBOSE,"Setting collaborators for all Methods \n");

  CFLog(VERBOSE,"Setting ConvergenceMethod collaborators for MeshAdapterMethods \n");
  SubSystem::setCollaborators<MeshAdapterMethod, ConvergenceMethod>(m_meshAdapterMethod, m_convergenceMethod);

  CFLog(VERBOSE,"Setting OutputFormatter collaborators for MeshAdapterMethods \n");
  SubSystem::setCollaborators<MeshAdapterMethod, OutputFormatter>(m_meshAdapterMethod, m_outputFormat);

  CFLog(VERBOSE,"Setting MeshCreator collaborators for MeshAdapterMethods \n");
  SubSystem::setCollaborators<MeshAdapterMethod, MeshCreator>(m_meshAdapterMethod, m_meshCreator);

  CFLog(VERBOSE,"Setting SpaceMethod collaborators for MeshAdapterMethods \n");
  SubSystem::setCollaborators<MeshAdapterMethod, SpaceMethod>(m_meshAdapterMethod, m_spaceMethod);

  CFLog(VERBOSE,"Setting LinearSystemSolver collaborators for ConvergenceMethods \n");
  SubSystem::setCollaborators<ConvergenceMethod, LinearSystemSolver>(m_convergenceMethod, m_linearSystemSolver);
  
  CFLog(VERBOSE,"Setting SpaceMethod collaborators for ConvergenceMethods \n");
  SubSystem::setCollaborators<ConvergenceMethod, SpaceMethod>(m_convergenceMethod, m_spaceMethod);
  
  CFLog(VERBOSE,"Setting LinearSystemSolver collaborators for SpaceMethods \n");
  SubSystem::setCollaborators<SpaceMethod, LinearSystemSolver>(m_spaceMethod, m_linearSystemSolver);

  CFLog(VERBOSE,"Setting ConvergenceMethods collaborators for SpaceMethods \n");
  SubSystem::setCollaborators<SpaceMethod, ConvergenceMethod>(m_spaceMethod, m_convergenceMethod);

  CFLog(VERBOSE,"Setting ConvergenceMethods collaborators for DataProcessingMethod (PRE) \n");
  SubSystem::setCollaborators<DataProcessingMethod, ConvergenceMethod>(m_dataPreProcessing, m_convergenceMethod);
  
  CFLog(VERBOSE,"Setting ConvergenceMethods collaborators for DataProcessingMethod (POST) \n");
  SubSystem::setCollaborators<DataProcessingMethod, ConvergenceMethod>(m_dataPostProcessing, m_convergenceMethod);
  
  CFLog(VERBOSE,"Setting LinearSystemSolver collaborators for DataProcessingMethod (PRE) \n");
  SubSystem::setCollaborators<DataProcessingMethod, LinearSystemSolver>(m_dataPreProcessing, m_linearSystemSolver);
  
  CFLog(VERBOSE,"Setting LinearSystemSolver collaborators for DataProcessingMethod (POST) \n");
  SubSystem::setCollaborators<DataProcessingMethod, LinearSystemSolver>(m_dataPostProcessing, m_linearSystemSolver);
}
    
//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::writeSolution(const bool force_write )
{
  CFAUTOTRACE;

  const int rank = Common::PE::GetPE().GetRank("Default");
  for (CFuint i = 0; i < m_outputFormat.size(); ++i)
  {
    if (PE::GetPE().isRankInGroup(rank, m_outputFormat[i]->getNamespace()) && 
       (!m_outputFormat[i]->isNonRootMethod()))
    {
      if(m_outputFormat[i]->isSaveNow( force_write ) )
      {
        Stopwatch<WallTime> stopTimer;
        stopTimer.start();
        m_outputFormat[i]->open ();
        m_outputFormat[i]->write();
        m_outputFormat[i]->close();
        stopTimer.stop();
        CFLog(INFO, "Writing took " << stopTimer << "s\n");
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::configurePhysicalModel ( Config::ConfigArgs& args )
{

  typedef std::vector<Common::SafePtr<Namespace> > NspVec;
  NspVec lst = NamespaceSwitcher::getInstance
    (SubSystemStatusStack::getCurrentName()).getAllNamespaces();
  
  cf_assert(!lst.empty());

  NspVec::iterator nsp = lst.begin();

  for(; nsp != lst.end(); ++nsp)
  {
    const std::string physicalModelName = (*nsp)->getPhysicalModelName();
    const std::string physicalModelType = (*nsp)->getPhysicalModelType();

    // create the new physical model implementor
    Common::SafePtr<PhysicalModelImpl::PROVIDER> physicalMdlProv =
      Environment::Factory<PhysicalModelImpl>::getInstance().getProvider(physicalModelType);

    cf_assert(physicalMdlProv.isNotNull());

    Common::SelfRegistPtr<PhysicalModelImpl> physicalModelImpl =
      physicalMdlProv->create(physicalModelName);

    cf_assert(physicalModelImpl.isNotNull());

    // configure the physical model implementor
    configureNested ( physicalModelImpl.getPtr(), args );

    // set the PhysicalModelImpl in the PhysicalModel
    // this phase has to be done after the configuration because
    // some data (example: nbEquations for models with multiple species)
    // could have required a configuration phase first
    PhysicalModelStack::getInstance().getEntry(physicalModelName)->setPhysicalModelImpl(physicalModelImpl);

    // default initialization of the equation subsystem descriptor
    const CFuint nbEqs = PhysicalModelStack::getInstance().getEntry(physicalModelName)->getNbEq();
    PhysicalModelStack::getInstance().getEntry(physicalModelName)->setEquationSubSysDescriptor(0,nbEqs,0);
  }
}
    
//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::writeConvergenceOnScreen()
{
  m_convergenceMethod.apply(mem_fun<void,ConvergenceMethod>(&ConvergenceMethod::writeOnScreen));
}

//////////////////////////////////////////////////////////////////////////////

void StandardSubSystem::dumpStates()
{
  // each mesh will dump its states
  vector <Common::SafePtr<MeshData> > meshDataVector = MeshDataStack::getInstance().getAllEntries();
  for (CFuint iMeshData = 0; iMeshData < meshDataVector.size(); iMeshData++) { 
    const std::string parStateVecName = meshDataVector[iMeshData]->getPrimaryNamespace() + "_states";
    DataHandle<State*, GLOBAL> states =
      meshDataVector[iMeshData]->getDataStorage()->getGlobalData<State*>(parStateVecName);
    
    const string fileName = "states.mesh-" + StringOps::to_str(iMeshData);
    
    // only rank 0 creates the file
    if (PE::GetPE().GetRank("Default") == 0) {
      ofstream file(fileName.c_str());
      // write out the states size
      file << states.size() << endl;
      file.close();
    }
    
    // be sure that all processes are ok
    PE::GetPE().setBarrier("Default");
    
    // ofstream fout(fileName.c_str(), ios::app);
    // Tamas: put here your algorithm to serialize the data (assume 1 mesh <==> 1 file)
    //        order should be based upon states[i]->getGlobalID() (same rule as in CFmesh files)
    
    // for (CFuint i = 0; i < states.size(); ++i) {
    //  fout << *states[i] << endl;
    // }
  }
}

//////////////////////////////////////////////////////////////////////////////

  } // namespace Framework

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////
