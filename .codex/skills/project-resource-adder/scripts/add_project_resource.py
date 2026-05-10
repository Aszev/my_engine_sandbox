#!/usr/bin/env python3
import argparse
import json
import os
import shutil
from datetime import datetime, timezone
from pathlib import Path


def load_json(path: Path) -> dict:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as stream:
        json.dump(data, stream, indent=2, ensure_ascii=False)
        stream.write("\n")


def resolve_asset_root(project_root: Path, explicit: str | None) -> Path:
    if explicit:
        return Path(explicit).expanduser().resolve()

    local_config = load_json(project_root / "project.local.json")
    asset_root = local_config.get("assetRoot", "")
    if not asset_root:
        raise SystemExit("assetRoot is missing. Ask the user for this machine's project asset directory.")

    path = Path(asset_root).expanduser()
    if not path.is_absolute():
        path = project_root / path
    return path.resolve()


def copy_resource(source: Path, target: Path) -> None:
    if source.is_dir():
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(source, target)
    else:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)


def main() -> int:
    parser = argparse.ArgumentParser(description="Copy a project resource into the external project asset store.")
    parser.add_argument("--project-root", default=".", help="Project repo root containing project.local.json.")
    parser.add_argument("--asset-root", help="Override external asset root.")
    parser.add_argument("--source", required=True, help="File or directory to import.")
    parser.add_argument("--dest", required=True, help="Destination path relative to assetRoot.")
    parser.add_argument("--source-url", default="", help="Original package or marketplace URL.")
    parser.add_argument("--license", default="", help="License name.")
    parser.add_argument("--author", default="", help="Asset author or vendor.")
    parser.add_argument("--notes", default="", help="Import notes.")
    args = parser.parse_args()

    project_root = Path(args.project_root).expanduser().resolve()
    asset_root = resolve_asset_root(project_root, args.asset_root)
    repo_assets = (project_root / "assets").resolve()
    if os.path.normcase(str(asset_root)) == os.path.normcase(str(repo_assets)):
        raise SystemExit("Refusing repo assets/ as assetRoot for heavy imports. Configure external OneDrive assetRoot.")

    source = Path(args.source).expanduser().resolve()
    if not source.exists():
        raise SystemExit(f"Source does not exist: {source}")

    relative_dest = Path(args.dest)
    if relative_dest.is_absolute() or ".." in relative_dest.parts:
        raise SystemExit("--dest must be a safe path relative to assetRoot.")

    target = (asset_root / relative_dest).resolve()
    if os.path.commonpath([str(asset_root), str(target)]) != str(asset_root):
        raise SystemExit("Destination escapes assetRoot.")

    copy_resource(source, target)

    store_root = asset_root.parent
    manifest_path = store_root / "catalog" / "project_assets.json"
    manifest = load_json(manifest_path)
    manifest.setdefault("version", 1)
    manifest.setdefault("project", project_root.name)
    manifest.setdefault("assetRoot", str(asset_root))
    assets = manifest.setdefault("assets", [])
    assets.append(
        {
            "path": str(relative_dest).replace("\\", "/"),
            "source": str(source),
            "sourceUrl": args.source_url,
            "license": args.license,
            "author": args.author,
            "notes": args.notes,
            "importedAt": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        }
    )
    write_json(manifest_path, manifest)

    print(f"Imported to: {target}")
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
