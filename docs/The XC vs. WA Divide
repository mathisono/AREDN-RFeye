The XC vs. WA Divide
As you've likely noticed, Ubiquiti split the 5AC generation into two distinct hardware trees:

XC Boards: The heavy-hitters (Gen1). These use the beefier 720MHz CPU (usually QCA955x based). Devices include the Rocket 5AC Lite, NanoBeam 5AC-19, and the PowerBeam 5AC 500.

WA Boards: The cost-optimized versions (Gen1 and Gen2). These step down to a 535MHz CPU with half the RAM (usually QCA956x based). Devices include the LiteBeam Gen2 and the lower-end PowerBeam 5AC-300/400.

Because the underlying SoCs are entirely different, the physical PCB traces—specifically how the RF Switch (RFS) taps the antenna path to feed the secondary airView radio—are routed differently. The 4CCCC marking you are looking at is almost certainly an SMD code for a GaAs/SOI RF switch IC (like a Skyworks or similar SPDT switch). You cannot use the WA trace map to figure out the XC board; the pinouts and switch logic GPIOs simply won't match up.

Why the PBE-5AC-500 Might Have the Missing Calibration Data
If you pulled a blank 0xFF template from the Rocket 5AC Lite, Mathison's theory about finding real calibration data on the PowerBeam 5AC 500 is highly credible for one specific reason: Antenna Integration.

Rocket 5AC Lite (Connectorized): This device ends in two RP-SMA connectors. Ubiquiti has no idea if the end-user is going to screw on a 13 dBi omni, a 22 dBi sector, or a 34 dBi dish. Because the antenna gain and physical path are unknown variables, they likely didn't bother populating granular phase/amplitude or band-edge calibration for the secondary airView radio. The generic fallback template was "good enough" for a listen-only spectrum analyzer.

PowerBeam 5AC 500 (Integrated): This device has a fixed, proprietary 27 dBi integrated feed horn and inner RF isolation shielding. Because the physical RF path, impedance, and antenna characteristics are 100% known and permanent from the factory, Ubiquiti's engineers may have actually populated the EEPROM/ART partition with real baseline noise floor calibrations and spur mitigation channels specific to that exact dish setup.
