# Porkchop pre-build script
# Ensures model files exist and generates version info

Import("env")
import os
import subprocess
from datetime import datetime

def get_git_commit():
    """Get short git commit hash, or 'unknown' if not in a git repo"""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except Exception:
        pass
    return "unknown"

def generate_build_info():
    """Write src/build_info.h with the current version, commit, and time."""
    build_info = {
        "build_time": datetime.now().isoformat(),
        "version": env.GetProjectOption("custom_version", "0.1.1"),
        "commit": get_git_commit()
    }

    info_path = os.path.join(env.get("PROJECT_SRC_DIR"), "build_info.h")
    with open(info_path, "w") as f:
        f.write("// Auto-generated build info\n")
        f.write("#pragma once\n")
        f.write(f'#define BUILD_TIME "{build_info["build_time"]}"\n')
        f.write(f'#define BUILD_VERSION "{build_info["version"]}"\n')
        f.write(f'#define BUILD_COMMIT "{build_info["commit"]}"\n')

    print(f"[pre_build] build_info.h stamped: v{build_info['version']} @ {build_info['commit']}")

# Generate NOW, during the pre phase (script import) — BEFORE any source is
# compiled. Registering this as an AddPreAction("buildprog", ...) instead fires
# it at LINK time, after display.cpp (which includes build_info.h) is already
# compiled, so the binary kept the stale committed commit hash.
generate_build_info()
