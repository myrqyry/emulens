# Shared OBS CI qualification

The shared reusable workflow is pinned to obs-plugin-ci 70ce37b. Its full
matrix builds OBS 32.2.0 and 33.0.3 for Linux x86-64, Windows x64 and native
macOS ARM64/Intel. The caller adapter owns SDK provisioning, effect assets
and packaging. This replaces the OBS33.0.1 compile-only workflow, not the
existing OBS32.1.0 minimum-version baseline. Both pipelines run during
qualification. No release is published.

Linux builds the pinned SDK with OpenGL, runs the nested CTest suite
(minimum 54 cases, mandatory named GPU/optical and memory tests, only the
pre-existing optional obspython skip), checks rundir and stages the real
module. Native Windows and macOS build against pinned native SDKs; they
do not yet run the Linux-specific OpenGL CTest suite. Native SDK bootstrap
adapted from Unscene 98c4453 and Streaming Media 875f6b1. Native OBS load,
rendering and notarization are not implied by archive success.

Shared test packages use emulens/bin/64bit/emulens.so or .dll and data/
for OBS32, emulens/emulens.so or .dll and data/ for OBS33, and emulens.plugin
on macOS. Each includes LICENSE and all tracked runtime assets. No PDBs
or duplicate module copies in these packages. Binary archives are
read back and checked against the staged tree before the shared checksum
and archive gate. Source tar.gz comes from the checked-out commit,
including public build scripts but excluding private/agent state, and is checked by the
existing source privacy verifier. Its separate PAYLOAD-SHA256SUMS
describes the Linux OBS32 binary package. Existing CPack canary tests
and traditional installers remain unchanged. Do not remove OBS32.1.0
baseline coverage until an equivalent shared minimum-version leg exists.

Hosted baseline 45908543420 passed all five jobs at 887b290; all three
downloaded runtime artifacts had all 63 assets (Windows CRLF only),
and the CPack source contained 232 verified entries. OBS33.0.1 run
45908543329 passed but had no artifact. Shared matrix qualification
is pending. Existing baseline artifacts lacked a bundled LICENSE and
included Windows PDBs; the shared packages correct those issues.

Local qualification: native installed OBS 32.2.0 SDK build, 53 of 54
CTest cases passed (only optional OBS-Python skipped), all six installer
safety cases passed, shared package contract tests passed. The staged
Linux32 archive has 65 regular files: the module, license, and all 63
runtime assets. OBS33 layout was checked using the same local binary;
actual OBS33 compilation and native Windows/macOS qualification await CI.
