"""Emit validated workflow metadata; release versions come only from Git tags."""
import json
import os
from pathlib import Path
from dependencies import ROOT, LOCK
from package import version_parts

if __name__ == "__main__":
    tag = os.environ.get("GITHUB_REF_NAME", "") if os.environ.get("GITHUB_REF_TYPE") == "tag" else ""
    if tag:
        if not tag.startswith("v"):
            raise ValueError("Release tags must start with v")
        version = tag[1:]
    else:
        version = (ROOT / "VERSION.txt").read_text().strip()
    version_parts(version)
    values = {"version": version, "qt": LOCK["qt"], "vcpkg": LOCK["vcpkg"]}
    if os.environ.get("GITHUB_OUTPUT"):
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
            output.writelines(f"{key}={value}\n" for key, value in values.items())
    print(json.dumps(values))
