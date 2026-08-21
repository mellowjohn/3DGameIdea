# Regenerate combat/UI one-shots with Foley-style SFX prompts (not near-peak noise).
# Requires HF auth for stabilityai/stable-audio-3-small-sfx

from __future__ import annotations

import json
from datetime import date
from pathlib import Path

import torch
import torchaudio
from stable_audio_3 import StableAudioModel

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "samples" / "open-world-rpg" / "assets" / "audio"
SCRATCH = Path(__file__).resolve().parent / "out"
PROVENANCE = OUT_DIR / "PROVENANCE.md"

# Short, concrete Foley briefs. TrackType: SFX matches SA3 prompting guide.
BATCH = [
    {
        "id": "sword_hit_wood",
        "file": "sword_hit_wood.wav",
        "duration": 0.55,
        "peak": 0.32,
        "seed": 1042,
        "prompt": (
            "TrackType: SFX, single one-shot: steel sword blade striking a wooden "
            "training dummy, sharp midrange crack then quick wooden thud, dry studio "
            "close mic, fast decay under 0.4 seconds, no whoosh, no reverb wash, "
            "no music, no ambience bed"
        ),
        "negative_prompt": "music, drone, choir, long reverb, explosion, laser, sci-fi",
    },
    {
        "id": "bow_release",
        "file": "bow_release.wav",
        "duration": 0.45,
        "peak": 0.26,
        "seed": 2217,
        "prompt": (
            "TrackType: SFX, single one-shot: wooden recurve bow string release, "
            "tight twang and soft string snap, brief air whoosh, dry field recording, "
            "very short decay, archery foley, no music, no impact thud"
        ),
        "negative_prompt": "music, explosion, metal clang, long reverb, synth",
    },
    {
        "id": "arrow_impact",
        "file": "arrow_impact.wav",
        "duration": 0.4,
        "peak": 0.3,
        "seed": 3381,
        "prompt": (
            "TrackType: SFX, single one-shot: arrow tip burying into a wooden target, "
            "short solid thunk with light wood knock, dry close mic, fast decay, "
            "archery impact foley, no music, no ricochet, no explosion"
        ),
        "negative_prompt": "music, glass, metal clang, long reverb, gunshot",
    },
    {
        "id": "ui_confirm",
        "file": "ui_confirm.wav",
        "duration": 0.28,
        "peak": 0.16,
        "seed": 4401,
        "prompt": (
            "TrackType: SFX, single one-shot: soft parchment UI confirm click, "
            "gentle wooden tap with tiny soft chime, very short and quiet, "
            "game menu interface, dry, no music, no voice"
        ),
        "negative_prompt": "loud, distorted, music, long reverb, sci-fi beep swarm",
    },
    {
        "id": "ui_hover",
        "file": "ui_hover.wav",
        "duration": 0.18,
        "peak": 0.1,
        "seed": 5510,
        "prompt": (
            "TrackType: SFX, single one-shot: tiny subtle UI hover tick, "
            "soft wood fingernail tap, extremely short and quiet, "
            "parchment menu, dry, no music"
        ),
        "negative_prompt": "loud, music, chime cascade, long reverb",
    },
]


def peak_normalize(audio: torch.Tensor, peak: float) -> torch.Tensor:
    max_abs = audio.abs().max().clamp_min(1e-8)
    return (audio / max_abs) * peak


def trim_silence(wav: torch.Tensor, sr: int, thresh: float = 0.02) -> torch.Tensor:
    """Drop leading/trailing near-silence so one-shots punch sooner."""
    mono = wav.abs().mean(dim=0)
    above = (mono > thresh).nonzero(as_tuple=False)
    if above.numel() == 0:
        return wav
    start = int(above[0].item())
    end = int(above[-1].item()) + 1
    pad = int(0.01 * sr)
    start = max(0, start - pad)
    end = min(wav.shape[-1], end + pad)
    return wav[:, start:end]


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    SCRATCH.mkdir(parents=True, exist_ok=True)

    print("Loading small-sfx…")
    model = StableAudioModel.from_pretrained("small-sfx")
    sample_rate = int(model.model.sample_rate)

    rows: list[dict] = []
    for item in BATCH:
        print(f"Generating {item['id']}…")
        audio = model.generate(
            prompt=item["prompt"],
            negative_prompt=item.get("negative_prompt"),
            duration=item["duration"],
            seed=int(item.get("seed", 0)),
            steps=8,
            cfg_scale=1.0,
        )
        wav = audio[0] if audio.dim() == 3 else audio
        wav = wav.float().cpu()
        wav = trim_silence(wav, sample_rate)
        wav = peak_normalize(wav, float(item["peak"]))

        scratch_path = SCRATCH / item["file"]
        out_path = OUT_DIR / item["file"]
        torchaudio.save(str(scratch_path), wav, sample_rate)
        torchaudio.save(str(out_path), wav, sample_rate)
        print(f"  wrote {out_path} ({wav.shape[-1] / sample_rate:.2f}s)")

        rows.append(
            {
                "id": item["id"],
                "file": item["file"],
                "prompt": item["prompt"],
                "duration_s": item["duration"],
                "peak": item["peak"],
                "sample_rate": sample_rate,
            }
        )

    (SCRATCH / f"batch_{date.today().isoformat()}_foley.json").write_text(
        json.dumps(rows, indent=2), encoding="utf-8"
    )

    lines = [
        "# Audio provenance",
        "",
        "Generated assets under this folder. Commercial use via Stability AI",
        "Community License (confirm Enterprise threshold if revenue > $1M/yr).",
        "",
        f"## Batch {date.today().isoformat()} - Stable Audio 3 Small-SFX (Foley pass)",
        "",
        "| File | Tool | Prompt | Notes |",
        "| --- | --- | --- | --- |",
    ]
    for r in rows:
        prompt = r["prompt"].replace("|", "/")
        lines.append(
            f"| `{r['file']}` | Stable Audio 3 `small-sfx` | {prompt} | "
            f"gen {r['duration_s']}s, peak {r['peak']}, trimmed, {r['sample_rate']} Hz |"
        )
    lines.extend(
        [
            "",
            "- Model: https://huggingface.co/stabilityai/stable-audio-3-small-sfx",
            "- Tooling: `tools/audio/generate_sfx_batch.py`",
            "- License: https://stability.ai/license",
            "- Prompt style: TrackType: SFX + short Foley one-shots (SA3 prompting guide)",
            "",
        ]
    )
    PROVENANCE.write_text("\n".join(lines), encoding="utf-8")
    print(f"Provenance updated: {PROVENANCE}")


if __name__ == "__main__":
    main()
