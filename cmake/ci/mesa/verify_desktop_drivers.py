"""Audit dlopen providers used by the six real desktop CTest processes (#31)."""
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re


def verify(evidence: Path, mesa_root: Path, salts_root: Path, vulkan: Path) -> dict:
    receipt = json.loads((evidence / "mesa-package.json").read_text())
    mesa_lib = (mesa_root / "lib").resolve(strict=True)
    salts_root = salts_root.resolve(strict=True)
    vulkan = vulkan.resolve(strict=True)
    expected_mesa = {name: (mesa_lib / name).resolve(strict=True) for name in receipt["libraries"]}
    for name, path in expected_mesa.items():
        assert path.parent == mesa_lib, path
        assert hashlib.sha256(path.read_bytes()).hexdigest() == receipt["libraries"][name], path
    expected_shapes = Counter({
        ("test_gcanvas_window_host", True, True): 1,
        ("test_gcanvas_plugin_window_host", True, False): 1,
        ("gcanvas_gpu_test", True, False): 1,
        ("gcanvas_gpu_test", False, True): 1,
        ("gcanvas_showcase", True, False): 1,
        ("gcanvas_showcase", False, True): 1,
    })
    binaries = {row[0] for row in expected_shapes}
    observed = Counter()
    records = []
    for trace in sorted(evidence.glob("loader.*")):
        text = trace.read_text()
        programs = re.findall(r"transferring control:\s+(\S+)", text)
        if len(programs) != 1 or Path(programs[0]).name not in binaries:
            continue  # CTest itself and its non-test subprocesses also have loader logs.
        name = Path(programs[0]).name
        initialized = Counter(Path(p).resolve(strict=True) for p in
                              re.findall(r"calling init:\s+(\S+)", text))
        finalized = Counter(Path(p).resolve(strict=True) for p in
                            re.findall(r"calling fini:\s+(\S+)", text))
        mesa = {p for p in initialized if p.name.startswith(("libGLX_mesa", "libgallium"))}
        assert not mesa or mesa == set(expected_mesa.values()), (trace, mesa)
        vk = {p for p in initialized if p.name.startswith("libvulkan_lvp")}
        assert not vk or vk == {vulkan}, (trace, vk)
        core = set()
        for path in initialized:
            if path.name.startswith("libsalts"):
                assert path.is_relative_to(salts_root), (trace, path)
            assert not re.match(r"libturbo.*\.(so|a)", path.name, re.I), (trace, path)
            if re.fullmatch(r"libsalts(?:[_-]core)?\.so(?:\.\d+)*", path.name, re.I):
                core.add(path)
        assert len(core) == (1 if name.startswith("test_") else 0), (trace, core)
        for path in mesa | vk:
            assert initialized[path] == finalized[path], (trace, path, initialized[path], finalized[path])
        observed[(name, bool(mesa), bool(vk))] += 1
        records.append({"program": programs[0], "trace": trace.name,
                        "mesa": sorted(str(p) for p in mesa),
                        "vulkan": sorted(str(p) for p in vk),
                        "core": sorted(str(p) for p in core)})
    assert observed == expected_shapes, (observed, expected_shapes)
    return {"processes": records, "mesa_package": receipt,
            "vulkan_sha256": hashlib.sha256(vulkan.read_bytes()).hexdigest()}


if __name__ == "__main__":
    evidence = Path(os.environ["EVIDENCE_DIR"])
    result = verify(evidence, Path(os.environ["MESA_ROOT"]),
                    Path(os.environ["SALTS_ROOT"]), Path(os.environ["VULKAN_EXPECTED_DRIVER"]))
    (evidence / "desktop-driver-providers.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
