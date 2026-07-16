# Milestone 2 Renderer Baseline

Date: 2026-07-02

This is a walking-skeleton measurement, not a production performance claim.

## Environment

- GPU: NVIDIA GeForce RTX 2080 SUPER
- API: Direct3D 12
- Build: Debug, MSVC 19.27, Windows SDK 10.0.18362.0
- Windowing: SDL 3.4.0
- Scene: runtime-compiled full-screen debug triangle pipeline

## Results

- Three-frame hidden smoke run: 131.24 FPS, 7.62 ms average CPU frame time.
- 60-frame hidden run at 1280×720: 145.72 FPS, 6.86 ms average CPU frame time.
- Timestamp-query verification: 0.0058 ms average GPU time for the debug triangle workload.
- 1280×720 capture completed and produced a valid PPM image.
- CTest foundation and real-GPU renderer smoke tests both pass.

VSync and per-frame GPU waiting intentionally constrain this diagnostic loop. Production frame pacing, frames in flight, timestamp queries, and 2560×1440 reference-scene measurements remain future work.
