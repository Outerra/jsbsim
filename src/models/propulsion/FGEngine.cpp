/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

 Module:       FGEngine.cpp
 Author:       Jon Berndt
 Date started: 01/21/99
 Called by:    FGAircraft

 ------------- Copyright (C) 1999  Jon S. Berndt (jon@jsbsim.org) -------------

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU Lesser General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA  02111-1307, USA.

 Further information about the GNU Lesser General Public License can also be found on
 the world wide web at http://www.gnu.org.

FUNCTIONAL DESCRIPTION
--------------------------------------------------------------------------------
See header file.

HISTORY
--------------------------------------------------------------------------------
01/21/99   JSB   Created
09/03/99   JSB   Changed Rocket thrust equation to correct -= Thrust instead of
                 += Thrust (thanks to Tony Peden)

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
INCLUDES
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "FGEngine.h"
#include "FGTank.h"
#include "FGPropeller.h"
#include "FGNozzle.h"
#include "FGRotor.h"
#include "input_output/FGXMLParse.h"
#include "math/FGColumnVector3.h"

using namespace std;

namespace JSBSim {

static const char *IdSrc = "$Id: FGEngine.cpp,v 1.52 2013/01/12 19:24:45 jberndt Exp $";
static const char *IdHdr = ID_ENGINE;

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CLASS IMPLEMENTATION
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

FGEngine::FGEngine(FGFDMExec* exec, Element* engine_element, int engine_number, struct Inputs& input)
                      : in(input), EngineNumber(engine_number)
{
  Element* local_element;
  FGColumnVector3 location, orientation;

  Name = "";
  Type = etUnknown;
  X = Y = Z = 0.0;
  EnginePitch = EngineYaw = 0.0;
  SLFuelFlowMax = 0.0;
  FuelExpended = 0.0;
  MaxThrottle = 1.0;
  MinThrottle = 0.0;

  ResetToIC(); // initialize dynamic terms

  FDMExec = exec;

  PropertyManager = FDMExec->GetPropertyManager();

  Name = engine_element->GetAttributeValue("name");

  Load(engine_element, PropertyManager, ::to_string(EngineNumber)); // Call ModelFunctions loader

// Find and set engine location

  local_element = engine_element->GetParent()->FindElement("location");
  if (local_element)  location = local_element->FindElementTripletConvertTo("IN");
//  else      cerr << "No engine location found for this engine." << endl;
// Jon: The engine location is not important - the nozzle location is.

  local_element = engine_element->GetParent()->FindElement("orient");
  if (local_element)  orientation = local_element->FindElementTripletConvertTo("RAD");
//  else          cerr << "No engine orientation found for this engine." << endl;
// Jon: The engine orientation has a default and is not normally used.

  SetPlacement(location, orientation);

  // Load thruster
  local_element = engine_element->GetParent()->FindElement("thruster");
  if (local_element) {
    try {
      if (!LoadThruster(local_element)) exit(-1);
    } catch (std::string str) {
      throw("Error loading engine " + Name + ". " + str);
    }
  } else {
    cerr << "No thruster definition supplied with engine definition." << endl;
  }

  // Load feed tank[s] references
  local_element = engine_element->GetParent()->FindElement("feed");
  while (local_element) {
    int tankID = (int)local_element->GetDataAsNumber();
    SourceTanks.push_back(tankID);
    local_element = engine_element->GetParent()->FindNextElement("feed");
  }

  string property_name, base_property_name;
  base_property_name = CreateIndexedPropertyName("propulsion/engine", EngineNumber);

  property_name = base_property_name + "/set-running";
  PropertyManager->Tie( property_name.c_str(), this, &FGEngine::GetRunning, &FGEngine::SetRunning );
  property_name = base_property_name + "/thrust-lbs";
  PropertyManager->Tie( property_name.c_str(), Thruster, &FGThruster::GetThrust);
  property_name = base_property_name + "/fuel-flow-rate-pps";
  PropertyManager->Tie( property_name.c_str(), this, &FGEngine::GetFuelFlowRate);
  property_name = base_property_name + "/fuel-flow-rate-gph";
  PropertyManager->Tie( property_name.c_str(), this, &FGEngine::GetFuelFlowRateGPH);
  property_name = base_property_name + "/fuel-used-lbs";
  PropertyManager->Tie( property_name.c_str(), this, &FGEngine::GetFuelUsedLbs);

  PostLoad(engine_element, PropertyManager, ::to_string(EngineNumber));

  Debug(0);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

FGEngine::~FGEngine()
{
  delete Thruster;
  Debug(1);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGEngine::ResetToIC(void)
{
  Starter = false;
  FuelExpended = 0.0;
  Starved = Running = Cranking = false;
  PctPower = 0.0;
  FuelFlow_gph = 0.0;
  FuelFlow_pph = 0.0;
  FuelFlowRate = 0.0;
  FuelFreeze = false;
  FuelUsedLbs = 0.0;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

double FGEngine::CalcFuelNeed(void)
{
  FuelFlowRate = SLFuelFlowMax*PctPower;
  FuelExpended = FuelFlowRate*in.TotalDeltaT;
  if (!Starved) FuelUsedLbs += FuelExpended;
  return FuelExpended;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

unsigned int FGEngine::GetSourceTank(unsigned int i) const
{
  if (i >= 0 && i < SourceTanks.size()) {
    return SourceTanks[i];
  } else {
    throw("No such source tank is available for this engine");
  }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGEngine::SetPlacement(const FGColumnVector3& location,
                            const FGColumnVector3& orientation)
{
  X = location(eX);
  Y = location(eY);
  Z = location(eZ);
  EnginePitch = orientation(ePitch);
  EngineYaw = orientation (eYaw);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

double FGEngine::GetThrust(void) const 
{
  return Thruster->GetThrust();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

const FGColumnVector3& FGEngine::GetBodyForces(void)
{
  return Thruster->GetBodyForces();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

const FGColumnVector3& FGEngine::GetMoments(void)
{
  return Thruster->GetMoments();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void FGEngine::LoadThrusterInputs()
{
  Thruster->in.TotalDeltaT     = in.TotalDeltaT;
  Thruster->in.H_agl           = in.H_agl;
  Thruster->in.PQR             = in.PQR;
  Thruster->in.AeroPQR         = in.AeroPQR;
  Thruster->in.AeroUVW         = in.AeroUVW;
  Thruster->in.Density         = in.Density;
  Thruster->in.Pressure        = in.Pressure;
  Thruster->in.Soundspeed      = in.Soundspeed;
  Thruster->in.Alpha           = in.alpha;
  Thruster->in.Beta            = in.beta;
  Thruster->in.Vt              = in.Vt;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

bool FGEngine::LoadThruster(Element *thruster_element)
{
  string token, fullpath, localpath;
  string thruster_filename, thruster_fullpathname, thrType;
  string enginePath = FDMExec->GetEnginePath();
  string aircraftPath = FDMExec->GetFullAircraftPath();
  ifstream thruster_file;
  FGColumnVector3 location, orientation;
  string separator = "/";

  fullpath = enginePath + separator;
  localpath = aircraftPath + separator + "Engines" + separator;

  thruster_filename = thruster_element->GetAttributeValue("file");
  if ( !thruster_filename.empty()) {
    thruster_fullpathname = localpath + thruster_filename + ".xml";
    thruster_file.open(thruster_fullpathname.c_str());
    if ( !thruster_file.is_open()) {
      thruster_fullpathname = fullpath + thruster_filename + ".xml";
      thruster_file.open(thruster_fullpathname.c_str());
      if ( !thruster_file.is_open()) {
        cerr << "Could not open thruster file: " << thruster_filename << ".xml" << endl;
        return false;
      } else {
        thruster_file.close();
      }
    } else {
      thruster_file.close();
    }
  } else {
    cerr << "No thruster filename given." << endl;
    return false;
  }

  document = LoadXMLDocument(thruster_fullpathname);
  document->SetParent(thruster_element);

  thrType = document->GetName();

  if (thrType == "propeller") {
    Thruster = new FGPropeller(FDMExec, document, EngineNumber);
  } else if (thrType == "nozzle") {
    Thruster = new FGNozzle(FDMExec, document, EngineNumber);
  } else if (thrType == "rotor") {
    Thruster = new FGRotor(FDMExec, document, EngineNumber);
  } else if (thrType == "direct") {
    Thruster = new FGThruster( FDMExec, document, EngineNumber);
  }

  Thruster->SetdeltaT(in.TotalDeltaT);

  Debug(2);
  return true;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//    The bitmasked value choices are as follows:
//    unset: In this case (the default) JSBSim would only print
//       out the normally expected messages, essentially echoing
//       the config files as they are read. If the environment
//       variable is not set, debug_lvl is set to 1 internally
//    0: This requests JSBSim not to output any messages
//       whatsoever.
//    1: This value explicity requests the normal JSBSim
//       startup messages
//    2: This value asks for a message to be printed out when
//       a class is instantiated
//    4: When this value is set, a message is displayed when a
//       FGModel object executes its Run() method
//    8: When this value is set, various runtime state variables
//       are printed out periodically
//    16: When set various parameters are sanity checked and
//       a message is printed out when they go out of bounds

void FGEngine::Debug(int from)
{
  if (debug_lvl <= 0) return;

  if (debug_lvl & 1) { // Standard console startup message output
    if (from == 0) { // Constructor

    }
    if (from == 2) { // After thruster loading
      cout << "      X = " << Thruster->GetLocationX() << endl;
      cout << "      Y = " << Thruster->GetLocationY() << endl;
      cout << "      Z = " << Thruster->GetLocationZ() << endl;
      cout << "      Pitch = " << radtodeg*Thruster->GetAnglesToBody(ePitch) << " degrees" << endl;
      cout << "      Yaw = " << radtodeg*Thruster->GetAnglesToBody(eYaw) << " degrees" << endl;
    }
  }
  if (debug_lvl & 2 ) { // Instantiation/Destruction notification
    if (from == 0) cout << "Instantiated: FGEngine" << endl;
    if (from == 1) cout << "Destroyed:    FGEngine" << endl;
  }
  if (debug_lvl & 4 ) { // Run() method entry print for FGModel-derived objects
  }
  if (debug_lvl & 8 ) { // Runtime state variables
  }
  if (debug_lvl & 16) { // Sanity checking
  }
  if (debug_lvl & 64) {
    if (from == 0) { // Constructor
      cout << IdSrc << endl;
      cout << IdHdr << endl;
    }
  }
}
}
