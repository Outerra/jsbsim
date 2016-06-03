# TestInitialConditions.py
#
# A regression test that checks that IC are correctly read from the IC file
# then loaded in the ic/ properties. It also checks that the correct ICs are
# reported in the data written in CSV files.
#
# Copyright (c) 2015 Bertrand Coconnier
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>
#

import os
import xml.etree.ElementTree as et
import pandas as pd
from JSBSim_utils import CreateFDM, append_xml, ExecuteUntil, JSBSimTestCase, RunTest

# Values copied from FGJSBBase.cpp and FGXMLElement.cpp
convtoft = {'FT': 1.0, 'M': 3.2808399, 'IN': 1.0/12.0}
convtofps = {'FT/SEC': 1.0, 'KTS': 1.68781}
convtodeg = {'DEG': 1.0, 'RAD': 57.295779513082320876798154814105}
convtokts = {'KTS': 1.0, 'FT/SEC': 1.0/1.68781}


class TestInitialConditions(JSBSimTestCase):
    def getElementTrees(self, s):
        # Read the IC file name from the script
        tree = et.parse(s)
        use_tag = tree.getroot().find('use')

        aircraft_name = use_tag.attrib['aircraft']
        aircraft_path = os.path.join('aircraft', aircraft_name)
        path_to_jsbsim_aircrafts = self.sandbox.path_to_jsbsim_file(aircraft_path)

        IC_file = append_xml(use_tag.attrib['initialize'])
        IC_tree = et.parse(os.path.join(path_to_jsbsim_aircrafts, IC_file))

        return (tree, IC_tree)

    def test_initial_conditions_v1(self):
        prop_output_to_CSV = ['velocities/vc-kts']
        # A dictionary that contains the XML tags to extract from the IC file
        # along with the name of the properties that contain the values
        # extracted from the IC file.
        vars = [{'tag': 'vt', 'unit': convtofps, 'default_unit': 'FT/SEC',
                 'ic_prop': 'ic/vt-fps', 'prop': 'velocities/vt-fps',
                 'CSV_header': 'V_{Total} (ft/s)'},
                {'tag': 'vc', 'unit': convtokts, 'default_unit': 'KTS',
                 'ic_prop': 'ic/vc-kts', 'prop': 'velocities/vc-kts',
                 'CSV_header': '/fdm/jsbsim/velocities/vc-kts'},
                {'tag': 'ubody', 'unit': convtofps, 'default_unit': 'FT/SEC',
                 'ic_prop': 'ic/u-fps', 'prop': 'velocities/u-fps',
                 'CSV_header': 'UBody'},
                {'tag': 'vbody', 'unit': convtofps, 'default_unit': 'FT/SEC',
                 'ic_prop': 'ic/v-fps', 'prop': 'velocities/v-fps',
                 'CSV_header': 'VBody'},
                {'tag': 'wbody', 'unit': convtofps, 'default_unit': 'FT/SEC',
                 'ic_prop': 'ic/w-fps', 'prop': 'velocities/w-fps',
                 'CSV_header': 'WBody'},
                {'tag': 'vnorth', 'unit': convtofps, 'default_unit': 'FT/SEC',
                 'ic_prop': 'ic/vn-fps', 'prop': 'velocities/v-north-fps',
                 'CSV_header': 'V_{North} (ft/s)'},
                {'tag': 'veast', 'unit': convtofps, 'default_unit': 'FT/SEC',
                 'ic_prop': 'ic/ve-fps', 'prop': 'velocities/v-east-fps',
                 'CSV_header': 'V_{East} (ft/s)'},
                {'tag': 'vdown', 'unit': convtofps, 'default_unit': 'FT/SEC',
                 'ic_prop': 'ic/vd-fps', 'prop': 'velocities/v-down-fps',
                 'CSV_header': 'V_{Down} (ft/s)'},
                {'tag': 'latitude', 'unit': convtodeg, 'default_unit': 'RAD',
                 'ic_prop': 'ic/lat-gc-deg', 'prop': 'position/lat-gc-deg',
                 'CSV_header': 'Latitude (deg)'},
                {'tag': 'longitude', 'unit': convtodeg, 'default_unit': 'RAD',
                 'ic_prop': 'ic/long-gc-deg', 'prop': 'position/long-gc-deg',
                 'CSV_header': 'Longitude (deg)'},
                {'tag': 'altitude', 'unit': convtoft, 'default_unit': 'FT',
                 'ic_prop': 'ic/h-agl-ft', 'prop': 'position/h-agl-ft',
                 'CSV_header': 'Altitude AGL (ft)'},
                {'tag': 'altitudeAGL', 'unit': convtoft, 'default_unit': 'FT',
                 'ic_prop': 'ic/h-agl-ft', 'prop': 'position/h-agl-ft',
                 'CSV_header': 'Altitude AGL (ft)'},
                {'tag': 'altitudeMSL', 'unit': convtoft, 'default_unit': 'FT',
                 'ic_prop': 'ic/h-sl-ft', 'prop': 'position/h-sl-ft',
                 'CSV_header': 'Altitude ASL (ft)'},
                {'tag': 'phi', 'unit': convtodeg, 'default_unit': 'RAD',
                 'ic_prop': 'ic/phi-deg', 'prop': 'attitude/phi-deg',
                 'CSV_header': 'Phi (deg)'},
                {'tag': 'theta', 'unit': convtodeg, 'default_unit': 'RAD',
                 'ic_prop': 'ic/theta-deg', 'prop': 'attitude/theta-deg',
                 'CSV_header': 'Theta (deg)'},
                {'tag': 'psi', 'unit': convtodeg, 'default_unit': 'RAD',
                 'ic_prop': 'ic/psi-true-deg', 'prop': 'attitude/psi-deg',
                 'CSV_header': 'Psi (deg)'},
                {'tag': 'elevation', 'unit': convtoft, 'default_unit': 'FT',
                 'ic_prop': 'ic/terrain-elevation-ft',
                 'prop': 'position/terrain-elevation-asl-ft',
                 'CSV_header': 'Terrain Elevation (ft)'}]

        for s in self.script_list(('ZLT-NT-moored-1.xml',
                                   '737_cruise_steady_turn_simplex.xml')):
            (tree, IC_tree) = self.getElementTrees(s)
            IC_root = IC_tree.getroot()

            # Only testing version 1.0 of init files
            if 'version' in IC_root.attrib and float(IC_root.attrib['version']) != 1.0:
                continue

            self.CheckICValues(vars, tree, IC_root, s, prop_output_to_CSV)

    def CheckICValues(self, vars, tree, IC_root, script_path,
                      prop_output_to_CSV=[]):
        # Extract the IC values from XML
        for var in vars:
            var_tag = IC_root.find(var['tag'])
            var['specified'] = var_tag is not None

            if var_tag is None:
                var['value'] = 0.0
                continue

            var['value'] = float(var_tag.text)
            if 'unit' in var_tag.attrib:
                conv = var['unit'][var_tag.attrib['unit']]
            else:
                conv = var['unit'][var['default_unit']]
            var['value'] *= conv

        # Generate a CSV file to check that it is correctly initialized
        # with the initial values
        output_tag = et.SubElement(tree.getroot(), 'output')
        output_tag.attrib['name'] = 'check_csv_values.csv'
        output_tag.attrib['type'] = 'CSV'
        output_tag.attrib['rate'] = '10'
        position_tag = et.SubElement(output_tag, 'position')
        position_tag.text = 'ON'
        velocities_tag = et.SubElement(output_tag, 'velocities')
        velocities_tag.text = 'ON'
        for props in prop_output_to_CSV:
            property_tag = et.SubElement(output_tag, 'property')
            property_tag.text = props
        f = os.path.split(script_path)[-1]  # Script name
        tree.write(f)

        # Initialize the script
        fdm = CreateFDM(self.sandbox)
        fdm.load_script(f)
        fdm.run_ic()

        # Sanity check, we just initialized JSBSim with the ICs, the time must
        # be set to 0.0
        self.assertEqual(fdm['simulation/sim-time-sec'], 0.0)

        # Check that the properties (including in 'ic/') have been correctly
        # initialized (i.e. that they contain the value read from the XML
        # file).
        for var in vars:
            if not var['specified']:
                continue

            value = var['value']
            prop = fdm[var['ic_prop']]
            if var['tag'] == 'psi':
                if abs(prop - 360.0) <= 1E-8:
                    prop = 0.0
            self.assertAlmostEqual(value, prop, delta=1E-7,
                                   msg="In script %s: %s should be %f but found %f" % (f, var['tag'], value, prop))
            prop = fdm[var['prop']]
            if var['tag'] == 'psi':
                if abs(prop - 360.0) <= 1E-8:
                    prop = 0.0
            self.assertAlmostEqual(value, prop, delta=1E-7,
                                   msg="In script %s: %s should be %f but found %f" % (f, var['tag'], value, prop))

        # Execute the first second of the script. This is to make sure that the
        # CSV file is open and the ICs have been written in it.
        try:
            ExecuteUntil(fdm, 1.0)
        except RuntimeError as e:
            if e.args[0] == 'Trim Failed':
                self.fail("Trim failed in script %s" % (f,))
            else:
                raise

        # Sanity check: make sure that the time step 0.0 has been copied in the
        # CSV file.
        ref = pd.read_csv('check_csv_values.csv')
        self.assertEqual(ref['Time'][0], 0.0)

        # Check that the value in the CSV file equals the value read from the
        # IC file.
        for var in vars:
            if not var['specified']:
                continue

            value = var['value']
            csv_value = ref[var['CSV_header']][0]
            if var['tag'] == 'psi':
                if abs(csv_value - 360.0) <= 1E-8:
                    csv_value = 0.0
            self.assertAlmostEqual(value, csv_value, delta=1E-7,
                                   msg="In script %s: %s should be %f but found %f" % (f, var['tag'], value, csv_value))

        del fdm

    def test_geod_position_from_init_file_v2(self):
        prop_output_to_CSV = ['position/geod-alt-ft']
        vars = [{'tag': 'latitude', 'unit': convtodeg, 'default_unit': 'RAD',
                 'ic_prop': 'ic/lat-geod-deg', 'prop': 'position/lat-geod-deg',
                 'CSV_header': 'Latitude Geodetic (deg)'},
                {'tag': 'longitude', 'unit': convtodeg, 'default_unit': 'RAD',
                 'ic_prop': 'ic/long-gc-deg', 'prop': 'position/long-gc-deg',
                 'CSV_header': 'Longitude (deg)'},
                {'tag': 'altitudeAGL', 'unit': convtoft, 'default_unit': 'FT',
                 'ic_prop': 'ic/geod-alt-ft', 'prop': 'position/geod-alt-ft',
                 'CSV_header': '/fdm/jsbsim/position/geod-alt-ft'},
                {'tag': 'altitudeMSL', 'unit': convtoft, 'default_unit': 'FT',
                 'ic_prop': 'ic/h-sl-ft', 'prop': 'position/h-sl-ft',
                 'CSV_header': 'Altitude ASL (ft)'}]

        for s in self.script_list(('ZLT-NT-moored-1.xml',
                                   '737_cruise_steady_turn_simplex.xml')):
            (tree, IC_tree) = self.getElementTrees(s)
            IC_root = IC_tree.getroot()

            # Only testing version 2.0 of init files
            if ('version' not in IC_root.attrib
                or float(IC_root.attrib['version']) != 2.0):
                continue

            position_tag = IC_root.find('position')
            lat_tag = position_tag.find('latitude')
            if lat_tag is None or 'type' not in lat_tag.attrib or lat_tag.attrib['type'][:4] != "geod":
                continue

            self.CheckICValues(vars, tree, position_tag, s, prop_output_to_CSV)

RunTest(TestInitialConditions)
