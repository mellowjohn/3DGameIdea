"""Normalize arbitrary glTF / GLB sources into the single-buffer form the bakers expect.

Blockbench exports already embed one base64 buffer plus one embedded PNG, which is what
`bake_tier1_props_gltf.read_accessor` / `load_atlas` assume. Files from other DCC tools may
split buffers into `.bin` sidecars, reference images by relative path, or arrive as `.glb`.
This module rewrites any of those into `<dest>.gltf` + `<dest>.png` so a single bake path works.
"""

from __future__ import annotations

import base64
import io
import json
import struct
from pathlib import Path
from typing import Any

from PIL import Image

GLB_MAGIC = 0x46546C67
GLB_CHUNK_JSON = 0x4E4F534A
GLB_CHUNK_BIN = 0x004E4942
DATA_URI_PREFIX = "data:"


def _decode_data_uri(uri: str) -> bytes:
    head, _, payload = uri.partition(",")
    if ";base64" in head:
        return base64.b64decode(payload)
    # Percent-encoded plain text payloads are legal but vanishingly rare; treat as UTF-8.
    return payload.encode("utf-8")


def _read_glb(path: Path) -> tuple[dict, bytes | None]:
    raw = path.read_bytes()
    if len(raw) < 12:
        raise ValueError(f"{path}: truncated GLB header")
    magic, _version, _length = struct.unpack_from("<III", raw, 0)
    if magic != GLB_MAGIC:
        raise ValueError(f"{path}: not a GLB container")
    offset = 12
    gltf: dict | None = None
    binary: bytes | None = None
    while offset + 8 <= len(raw):
        chunk_len, chunk_type = struct.unpack_from("<II", raw, offset)
        body = raw[offset + 8 : offset + 8 + chunk_len]
        if chunk_type == GLB_CHUNK_JSON:
            gltf = json.loads(body.decode("utf-8"))
        elif chunk_type == GLB_CHUNK_BIN:
            binary = bytes(body)
        offset += 8 + chunk_len + ((4 - (chunk_len % 4)) % 4)
    if gltf is None:
        raise ValueError(f"{path}: GLB has no JSON chunk")
    return gltf, binary


def load_source(path: Path) -> tuple[dict, list[bytes]]:
    """Return the glTF JSON plus the resolved bytes for every declared buffer."""
    suffix = path.suffix.lower()
    if suffix == ".glb":
        gltf, binary = _read_glb(path)
        buffers = []
        for buffer in gltf.get("buffers") or []:
            uri = buffer.get("uri")
            if uri is None:
                buffers.append(binary or b"")
            elif uri.startswith(DATA_URI_PREFIX):
                buffers.append(_decode_data_uri(uri))
            else:
                buffers.append((path.parent / uri).read_bytes())
        return gltf, buffers

    gltf = json.loads(path.read_text(encoding="utf-8"))
    buffers = []
    for buffer in gltf.get("buffers") or []:
        uri = buffer.get("uri")
        if not uri:
            raise ValueError(f"{path}: .gltf buffer without uri (GLB-only feature)")
        if uri.startswith(DATA_URI_PREFIX):
            buffers.append(_decode_data_uri(uri))
        else:
            buffers.append((path.parent / uri).read_bytes())
    return gltf, buffers


def merge_buffers(gltf: dict, buffers: list[bytes]) -> bytes:
    """Collapse every buffer into buffer 0 and rewrite bufferView offsets in place."""
    if not buffers:
        return b""
    if len(buffers) == 1:
        gltf["buffers"] = [{"byteLength": len(buffers[0])}]
        for view in gltf.get("bufferViews") or []:
            view["buffer"] = 0
        return buffers[0]

    blob = bytearray()
    bases = []
    for chunk in buffers:
        bases.append(len(blob))
        blob += chunk
        blob += b"\x00" * ((4 - (len(blob) % 4)) % 4)
    for view in gltf.get("bufferViews") or []:
        index = view.get("buffer", 0)
        view["byteOffset"] = view.get("byteOffset", 0) + bases[index]
        view["buffer"] = 0
    gltf["buffers"] = [{"byteLength": len(blob)}]
    return bytes(blob)


def resolve_image(gltf: dict, buffers: list[bytes], source: Path, index: int = 0) -> Image.Image | None:
    """Decode one glTF image (data uri, external file, or bufferView) into RGBA."""
    images = gltf.get("images") or []
    if index >= len(images):
        sidecar = source.with_suffix(".png")
        if sidecar.exists():
            return Image.open(sidecar).convert("RGBA")
        return None
    image = images[index]
    uri = image.get("uri")
    if uri:
        if uri.startswith(DATA_URI_PREFIX):
            return Image.open(io.BytesIO(_decode_data_uri(uri))).convert("RGBA")
        return Image.open((source.parent / uri).resolve()).convert("RGBA")
    view_index = image.get("bufferView")
    if view_index is None:
        return None
    view = (gltf.get("bufferViews") or [])[view_index]
    data = buffers[view.get("buffer", 0)]
    start = view.get("byteOffset", 0)
    return Image.open(io.BytesIO(data[start : start + view["byteLength"]])).convert("RGBA")


def embed_buffer(gltf: dict, blob: bytes) -> None:
    encoded = base64.b64encode(blob).decode("ascii")
    gltf["buffers"] = [
        {"byteLength": len(blob), "uri": "data:application/octet-stream;base64," + encoded}
    ]


def normalize(source: Path, dest_gltf: Path) -> dict[str, Any]:
    """Write `dest_gltf` (+ sidecar PNG) as an embedded-buffer glTF the bakers can read directly."""
    gltf, buffers = load_source(source)
    source_image_count = len(gltf.get("images") or [])
    atlas = resolve_image(gltf, buffers, source)
    blob = merge_buffers(gltf, buffers)
    embed_buffer(gltf, blob)

    dest_gltf.parent.mkdir(parents=True, exist_ok=True)
    png_name = dest_gltf.stem + ".png"
    dest_png = dest_gltf.with_name(png_name)
    if atlas is not None:
        atlas.save(dest_png, format="PNG")
        gltf["images"] = [{"uri": png_name}]
        gltf["textures"] = [{"source": 0, "sampler": 0}]
        gltf.setdefault("samplers", [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}])
        for material in gltf.get("materials") or []:
            pbr = material.setdefault("pbrMetallicRoughness", {})
            pbr["baseColorTexture"] = {"index": 0}
    dest_gltf.write_text(json.dumps(gltf, separators=(",", ":")), encoding="utf-8")

    return {
        "gltf": str(dest_gltf),
        "atlas": str(dest_png) if atlas is not None else None,
        "atlasSize": list(atlas.size) if atlas is not None else None,
        "sourceImageCount": source_image_count,
        "skins": len(gltf.get("skins") or []),
        "clips": [a.get("name") or f"clip_{i}" for i, a in enumerate(gltf.get("animations") or [])],
        "materialCount": len(gltf.get("materials") or []),
    }


def measure_height(gltf: dict) -> float:
    """Authored Y extent from POSITION accessor bounds (glTF requires min/max on POSITION)."""
    accessors = gltf.get("accessors") or []
    lo, hi = None, None
    for mesh in gltf.get("meshes") or []:
        for prim in mesh.get("primitives") or []:
            index = (prim.get("attributes") or {}).get("POSITION")
            if index is None or index >= len(accessors):
                continue
            accessor = accessors[index]
            if "min" not in accessor or "max" not in accessor:
                continue
            lo = accessor["min"][1] if lo is None else min(lo, accessor["min"][1])
            hi = accessor["max"][1] if hi is None else max(hi, accessor["max"][1])
    if lo is None or hi is None or hi <= lo:
        return 0.0
    return float(hi - lo)
