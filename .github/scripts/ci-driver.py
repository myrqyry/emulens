"""Caller-owned EmuLens build/test/package adapter for obs-plugin-ci."""
import importlib.util
import os
import plistlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("native_sdk", Path(__file__).with_name("native-sdk.py"))
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)
run = sdk.run
digest = sdk.digest

def files(root):
    paths = list(root.rglob("*"))
    if any(p.is_symlink() for p in paths):
        raise RuntimeError(f"Unexpected symlink in {root}")
    return {p.relative_to(root).as_posix(): p for p in paths if p.is_file()}

def assets():
    # Only tracked runtime files; source-only guidance never ships.
    tracked = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "data"], text=True).splitlines()
    return {Path(p).relative_to("data").as_posix(): ROOT / p for p in tracked if Path(p).name != "AGENTS.md"}

def verify_assets(data):
    expected = assets()
    actual = files(data)
    if set(actual) != set(expected) or any(actual[n].read_bytes() != expected[n].read_bytes() for n in expected):
        raise RuntimeError(f"Installed asset mismatch: {set(actual) ^ set(expected)}")

def prepare_payload(stage, package, target, version):
    sdk.sdk_pin(version)
    if target == "linux-x86_64":
        matches = list(stage.glob("lib*/**/obs-plugins/emulens.so"))
        if len(matches) != 1:
            raise RuntimeError(f"Expected one installed Linux module, got {matches}")
        binary = matches[0]
        data = stage / "share/obs/obs-plugins/emulens"
    elif target == "windows-x64":
        binary = stage / "emulens/emulens.dll"
        legacy = stage / "emulens/bin/64bit/emulens.dll"
        if legacy.read_bytes() != binary.read_bytes():
            raise RuntimeError("Windows legacy and modern staged binaries differ")
        data = stage / "emulens/data"
    elif target in ("macos-arm64", "macos-x86_64"):
        bundle = stage / "emulens.plugin"
        data = bundle / "Contents/Resources"
        verify_assets(data)
        binary = bundle / "Contents/MacOS/emulens"
        if not binary.is_file() or not binary.stat().st_size:
            raise RuntimeError("Missing native module")
        info = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
        if info.get("CFBundleExecutable") != "emulens" or info.get("CFBundlePackageType") != "BNDL":
            raise RuntimeError("Invalid plugin bundle metadata")
        files(bundle)
        shutil.copytree(bundle, package / bundle.name)
        result = package / bundle.name / "Contents/MacOS/emulens"
        shutil.copy2(ROOT / "LICENSE", package / bundle.name / "Contents/Resources/LICENSE")
        return result
    else:
        raise RuntimeError(f"Unsupported target {target}")
    verify_assets(data)
    if not binary.is_file() or not binary.stat().st_size:
        raise RuntimeError("Missing native module")
    dest = package / "emulens"
    module = dest / ("bin/64bit" if version == sdk.DEFAULT_VERSION else "") / binary.name
    module.parent.mkdir(parents=True)
    shutil.copy2(binary, module)
    shutil.copytree(data, dest / "data")
    shutil.copy2(ROOT / "LICENSE", dest / "LICENSE")
    return module

def manifest(paths):
    return "".join(f"{digest(path)}  {name}\n" for name, path in sorted(paths.items()))

def package_binary(package, archive, target):
    paths = files(package)
    if target == "linux-x86_64":
        with tarfile.open(archive, "w:gz") as tar:
            for name, path in sorted(paths.items()):
                tar.add(path, arcname=name, recursive=False)
        with tarfile.open(archive, "r:gz") as tar:
            members = tar.getmembers()
            if len(members) != len(paths) or {m.name for m in members} != set(paths) or any(
                not m.isfile() or tar.extractfile(m).read() != paths[m.name].read_bytes() for m in members
            ):
                raise RuntimeError("Binary tar payload mismatch")
    else:
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as z:
            for name, path in sorted(paths.items()):
                z.write(path, name)
        with zipfile.ZipFile(archive) as z:
            if len(z.namelist()) != len(paths) or set(z.namelist()) != set(paths) or z.testzip() or any(
                z.read(name) != path.read_bytes() for name, path in paths.items()
            ):
                raise RuntimeError("Binary ZIP payload mismatch")
    (archive.parent / "SHA256SUMS").write_text(manifest({archive.name: archive}), encoding="utf-8", newline="\n")
    return paths

def source_package(work, payload):
    # Commit-owned public sources only; the retained baseline CI also tests the
    # CPack working-tree privacy canaries independently.
    work.mkdir()
    archive = work / "emulens-source.tar.gz"
    excluded = (".meristem",)
    tracked = subprocess.check_output(["git", "-C", str(ROOT), "ls-files"], text=True).splitlines()
    allowed = [p for p in tracked if p.split("/")[0] not in excluded and Path(p).name not in
               {"AGENTS.md", "ACTIVE_WORK.md", "SESSION-STATE.md"}]
    run("git", "-C", ROOT, "archive", "--format=tar.gz", "--prefix=emulens/", "-o", archive, "HEAD", "--", *allowed)
    run("python3", ROOT / "tests/check_source_archive.py", archive)
    (work / "SHA256SUMS").write_text(manifest({archive.name: archive}), encoding="utf-8", newline="\n")
    (work / "PAYLOAD-SHA256SUMS").write_text(manifest(payload), encoding="utf-8", newline="\n")

def main():
    target, version, archive_name = (os.environ[k] for k in ("TARGET", "OBS_VERSION", "ARCHIVE"))
    if os.environ["MODULE"] != "emulens" or os.environ["PUBLIC_NAME"] != "emulens":
        raise RuntimeError("Wrong caller module")
    extension = "tar.gz" if target == "linux-x86_64" else "zip"
    if archive_name != f"emulens-{target}-obs{version}-test.{extension}":
        raise RuntimeError("Wrong archive name")
    sdk.sdk_pin(version)
    work = Path(os.environ["RUNNER_TEMP"]) / "plugin-ci"
    work.mkdir()
    build = work / "build"
    stage = work / "stage"
    if target == "linux-x86_64":
        sdk_work = work / "obs"
        run("bash", ROOT / ".github/scripts/prepare-obs32-sdk.sh", sdk_work, version)
        prefix = sdk_work / "sdk"
        generator = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo"]
        deps = []
    else:
        host, generator = sdk.native_config()
        if host != target:
            raise RuntimeError("Wrong native host")
        obs = sdk.source_archive(work, version)
        sdk.patch_obs_sdk(obs, target, version)
        prefix = work / "sdk"
        obs_build = work / "obs-build"
        run("cmake", "-S", obs, "-B", obs_build, *generator,
            "-DOBS_CMAKE_VERSION:STRING=3.0.0", f"-DOBS_VERSION_OVERRIDE:STRING={version}",
            "-DENABLE_PLUGINS=OFF", "-DENABLE_FRONTEND=OFF", "-DENABLE_BROWSER=OFF", "-DENABLE_SCRIPTING=OFF")
        run("cmake", "--build", obs_build, "--config", "Release", "--target", "obs-frontend-api", "--parallel", "4")
        run("cmake", "--install", obs_build, "--config", "Release", "--component", "Development", "--prefix", prefix)
        deps = list(sdk.dependency_dirs(obs, target, version))
    run("cmake", "-S", ROOT, "-B", build, *generator,
        "-DEMULENS_EXTERNAL_SDK=ON", "-DEMULENS_BUILD_OBS_FROM_SOURCE=OFF",
        "-DENABLE_QT=OFF", "-DENABLE_FRONTEND_API=OFF",
        f"-DENABLE_TESTING={'ON' if target == 'linux-x86_64' else 'OFF'}",
        f"-Dlibobs_DIR={sdk.config_dir(prefix, 'libobs').as_posix()}",
        "-DCMAKE_PREFIX_PATH=" + ";".join(p.as_posix() for p in [prefix, *deps]))
    run("cmake", "--build", build, "--config", "RelWithDebInfo", "--parallel", "4")
    if target == "linux-x86_64":
        if list((build / "rundir").rglob("AGENTS.md")):
            raise RuntimeError("Source-only guidance in rundir")
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = str(prefix / "lib")
        env["LIBGL_ALWAYS_SOFTWARE"] = "1"
        report = work / "ctest.xml"
        run("ctest", "--test-dir", build / "tests", "--output-on-failure", "--no-tests=error", "--output-junit", report, env=env)
        cases = ET.parse(report).findall(".//testcase")
        required = {"WetLensGpuRenderTest", "PhoneWetGpuRenderTest", "MemLeakTest", "OpticalFlareGpuRenderTest", "OpticalFlareContractTest", "HalationGpuRenderTest", "RecoveredOpticsGpuRenderTest"}
        skipped = {c.get("name") for c in cases if c.find("skipped") is not None or c.get("status") in ("notrun", "disabled")}
        if len(cases) < int(os.environ["MINIMUM_TESTS"]) or not required <= {c.get("name") for c in cases} or skipped - {"PythonIntegrationTest"} or any(c.find("failure") is not None or c.find("error") is not None for c in cases):
            raise RuntimeError("Incomplete native/GPU suite")
        print(f"{len(cases)} CTest cases; optional skips: {sorted(skipped)}")
    run("cmake", "--install", build, "--config", "RelWithDebInfo", "--prefix", stage)
    package = work / "payload"
    binary = prepare_payload(stage, package, target, version)
    if target.startswith("macos"):
        run(*sdk.mac_arch_command(binary, target))
        run("codesign", "--force", "--sign", "-", package / "emulens.plugin")
        run("codesign", "--verify", "--strict", package / "emulens.plugin")
    elif target == "windows-x64":
        with binary.open("rb") as f:
            if f.read(2) != b"MZ":
                raise RuntimeError("Invalid PE module")
            f.seek(0x3c)
            f.seek(int.from_bytes(f.read(4), "little"))
            if f.read(6) != b"PE\0\0\x64\x86":
                raise RuntimeError("Not Windows x64")
    else:
        exports = subprocess.check_output(["nm", "-D", "--defined-only", str(binary)], text=True)
        if not {"obs_module_load", "obs_module_set_pointer", "obs_module_ver"} <= {line.split()[-1] for line in exports.splitlines() if line.split()}:
            raise RuntimeError("Missing required OBS module exports")
    payload = package_binary(package, work / archive_name, target)
    if target == "linux-x86_64" and version == sdk.DEFAULT_VERSION:
        source_package(work.parent / "plugin-ci-source", payload)

if __name__ == "__main__":
    main()
