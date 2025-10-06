# Midterm Report — Animated Fire & Smoke Visualization using 3D Perlin Noise
*(Final Project template alignment)*

**Author:** <Your Name>  
**Course:** Computer Graphics / Visualization  
**Date:** 2025-10-06  
**GitHub:** <replace with your repository URL>

---

## 1. Introduction (with Background)
Real‑time visualization of natural phenomena such as fire and smoke is an enduring topic in computer graphics. Physically based simulation (Navier‑Stokes, combustion chemistry) is often too heavy for interactive applications. A common alternative is *procedural texturing* driven by stochastic fields (e.g., Perlin noise) to approximate appearance and motion while keeping costs low.

In this prototype, we render a campfire‑like flame and a rising smoke column using a single **3D Perlin noise** texture (fBm). Two billboard passes (fire & smoke) sample the same volume with different masks, color mapping and blend modes. This achieves a visually convincing effect at negligible geometry cost and without particle systems.

### Background (Literature review short)
- *Perlin (1985, 2002)* introduced gradient noise and turbulence; it remains a staple for procedural fire/smoke.
- *GPU Gems* (NVIDIA) chapters on volumetric effects show practical pipelines using 3D noise and billboards.
- *PBRT* and *Bridson* cover physically‑based alternatives; we target a lightweight procedural model instead.

## 2. Project Description
**Goal.** Produce an animated campfire with smoke (as in the reference photo) using OpenGL 3.3 + GLFW + GLAD in a single `main.cpp`.  
**Key idea.** Precompute a small 3D noise volume on CPU and animate by advecting sampling coordinates (temporal scroll + lateral wobble). Render two quads (one for fire, one for smoke), each with its own mask and color mapping.

### Visual requirements
- Flame tapers to a rounded tip; no center seams.  
- Fire colors approximate blackbody: red→orange→yellow→near‑white core.  
- Smoke is a rectangular, slightly wavy, semi‑transparent column.

## 3. Proposal
### Problem statement
How to render plausible fire & smoke in real time without particles or heavy simulation, suitable for mid‑range GPUs and simple code (one file).

### Objectives
1. CPU‑generated 3D fBm Perlin texture and efficient upload to `GL_R8` 3D texture.  
2. Two‑pass billboard renderer with correct blending: **additive** for fire, **alpha** for smoke.  
3. Robust masks that avoid center seams; rounded flame tip.  
4. Color ramp approximating real flame (hot white/yellow core).  
5. Clean code and controls for tuning; report & slides; GitHub project.

### Preliminary tech stack
C++17, OpenGL 3.3 Core, GLFW, GLAD, GLSL 330, CMake (optional).

## 4. Methodology
1. **Noise volume.** Generate fBm: `sum(octaves) noise(freq)*amp` with lacunarity≈2.0, gain≈0.5. Store as 8‑bit `GL_R8` 3D texture (96³).  
2. **Billboard geometry.** Single vertical quad; vertex shader scales by width/height and anchors to screen bottom.  
3. **Fire shader.**
   - **Mask:** cone half‑width `halfW(y)` (wide at bottom → narrow at top), no seam: `1 - smoothstep(halfW-edge, halfW, dx)`; rounded top via *outside‑circle* mask; final: `min(cone, capOutside)`.
   - **Animation:** temporal scroll in noise `z = time*speed` + lateral wobble near the tip.
   - **Coloring:** blackbody‑inspired ramp; extra “core whitening” at the center near the base.
   - **Blend:** `SRC_ALPHA, ONE` (additive) for emissive look.
4. **Smoke shader.**
   - **Mask:** rectangular width with wavy centerline, seam‑free mask.  
   - **Animation:** slower scroll & slight horizontal waving; density decreases with height.  
   - **Blend:** `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`.
5. **Controls.** Hotkeys to scale/speed/height for quick tuning.  
6. **Validation.** Visual comparison to reference; check for artifacts (center lines, hard 90° edges).

## 5. Prototype (basic loop + one feature)
- Basic loop: GLFW window → GLAD init → noise upload → two draw calls per frame.  
- **One feature implemented:** *3D Perlin‑driven fire with rounded tip*, including improved physical color ramp and no seam artifacts. Smoke uses the same noise with different mapping.

## 6. Expected Results & Metrics
- 60 FPS on integrated GPUs (texture 96³).  
- Visual plausibility: smooth, flickering flame, soft smoke column, continuous transition at base.

## 7. Risks & Mitigation
- **Color look‑dev** varies by monitor → expose intensity & ramp params.  
- **Bandwidth on mobile GPUs** → keep volume ≤128³, reduce octaves.  
- **Blend order artifacts** → draw fire first (additive), then smoke (alpha).

## 8. Timeline (remaining)
- **Week 6–7:** sparks (particles) and heat‑haze (screen‑space refraction).  
- **Week 8:** polishing, screenshots/video, final write‑up.  
- **Week 9:** final presentation & repo cleanup.

## 9. References
- K. Perlin, “An Image Synthesizer”, SIGGRAPH 1985.  
- K. Perlin, “Improving Noise”, SIGGRAPH 2002.  
- M. Pharr, W. Jakob, G. Humphreys, *Physically Based Rendering*.  
- R. Bridson, *Fluid Simulation for Computer Graphics*.  
- GPU Gems (NVIDIA) — volumetric effects & noise chapters.

---

> **Submission checklist:** report (this doc), presentation (`slides_midterm_marp.md`), and GitHub link to the prototype repository.
