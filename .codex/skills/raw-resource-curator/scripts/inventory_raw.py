#!/usr/bin/env python3
import argparse
import csv
import os
import struct
import zipfile
from collections import Counter, defaultdict
from pathlib import Path


def png_size(path: Path):
    with path.open("rb") as f:
        header = f.read(24)
    if header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", header[16:24])


def jpg_size(path: Path):
    with path.open("rb") as f:
        if f.read(2) != b"\xff\xd8":
            return None
        while True:
            marker_start = f.read(1)
            if not marker_start:
                return None
            if marker_start != b"\xff":
                continue
            marker = f.read(1)
            while marker == b"\xff":
                marker = f.read(1)
            if marker in [
                b"\xc0", b"\xc1", b"\xc2", b"\xc3", b"\xc5", b"\xc6",
                b"\xc7", b"\xc9", b"\xca", b"\xcb", b"\xcd", b"\xce", b"\xcf",
            ]:
                f.read(2)
                f.read(1)
                height, width = struct.unpack(">HH", f.read(4))
                return width, height
            length_bytes = f.read(2)
            if len(length_bytes) != 2:
                return None
            length = struct.unpack(">H", length_bytes)[0]
            f.seek(length - 2, os.SEEK_CUR)


def image_size(path: Path):
    try:
        if path.suffix.lower() == ".png":
            return png_size(path)
        if path.suffix.lower() in [".jpg", ".jpeg"]:
            return jpg_size(path)
    except Exception:
        return None
    return None


def rel(path: Path, root: Path):
    return path.relative_to(root).as_posix()


def read_manifest_head(path: Path, limit=8):
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as f:
            rows = []
            for index, row in enumerate(csv.reader(f)):
                rows.append(row)
                if index + 1 >= limit:
                    break
            return rows
    except Exception:
        return []


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = Path(args.root).resolve()
    output = Path(args.output)
    files = [p for p in root.rglob("*") if p.is_file()]
    dirs = [p for p in root.rglob("*") if p.is_dir()]

    by_ext = Counter(p.suffix.lower() or "<none>" for p in files)
    by_dir = defaultdict(list)
    for path in files:
        by_dir[path.parent].append(path)

    lines = [
        "# Raw Assets Inventory",
        "",
        f"Root: `{root}`",
        "",
        "## Summary",
        "",
    ]
    total = sum(path.stat().st_size for path in files)
    lines.append(f"- Files: {len(files)}")
    lines.append(f"- Directories: {len(dirs)}")
    lines.append(f"- Size: {total / 1024 / 1024:.2f} MB")
    lines.append("")
    lines.append("| Extension | Count |")
    lines.append("| --- | ---: |")
    for ext, count in sorted(by_ext.items(), key=lambda item: (-item[1], item[0])):
        lines.append(f"| `{ext}` | {count} |")

    lines.append("")
    lines.append("## Directories")
    lines.append("")
    lines.append("| Directory | Files | Size MB | Extensions |")
    lines.append("| --- | ---: | ---: | --- |")
    for directory in sorted(by_dir):
        group = by_dir[directory]
        size = sum(path.stat().st_size for path in group) / 1024 / 1024
        exts = Counter(path.suffix.lower() or "<none>" for path in group)
        ext_text = ", ".join(f"`{k}`:{v}" for k, v in sorted(exts.items()))
        lines.append(f"| `{rel(directory, root) or '.'}` | {len(group)} | {size:.2f} | {ext_text} |")

    archives = [path for path in files if path.suffix.lower() == ".zip"]
    if archives:
        lines.append("")
        lines.append("## Archives")
        for archive in archives:
            lines.append("")
            lines.append(f"### `{rel(archive, root)}`")
            lines.append("")
            try:
                with zipfile.ZipFile(archive) as zip_file:
                    groups = Counter(
                        str(Path(info.filename).parent).replace("\\", "/")
                        for info in zip_file.infolist()
                        if not info.is_dir()
                    )
                lines.append("| Archive folder | Entries |")
                lines.append("| --- | ---: |")
                for folder, count in sorted(groups.items()):
                    lines.append(f"| `{folder}` | {count} |")
            except Exception as exc:
                lines.append(f"Could not inspect archive: {exc}")

    images = [path for path in files if path.suffix.lower() in [".png", ".jpg", ".jpeg"]]
    if images:
        lines.append("")
        lines.append("## Image Files")
        lines.append("")
        lines.append("| File | Size | KB |")
        lines.append("| --- | --- | ---: |")
        for image in sorted(images):
            size = image_size(image)
            size_text = f"{size[0]}x{size[1]}" if size else "unknown"
            lines.append(f"| `{rel(image, root)}` | {size_text} | {image.stat().st_size / 1024:.1f} |")

    manifests = [path for path in files if path.name.lower() == "source_manifest.csv"]
    if manifests:
        lines.append("")
        lines.append("## Source Manifests")
        for manifest in sorted(manifests):
            lines.append("")
            lines.append(f"### `{rel(manifest, root)}`")
            rows = read_manifest_head(manifest)
            if rows:
                lines.append("")
                lines.append("```csv")
                for row in rows:
                    lines.append(",".join(row))
                lines.append("```")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
