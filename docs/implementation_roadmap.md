# Gritbaal Synth — Implementation Roadmap & Architectural Guidelines

This document outlines the step-by-step implementation strategy, technical constraints, performance requirements, and visual rendering principles for the **Gritbaal Synth** project.

---

## 1. Technical Constraints & Architecture Directives

### 1.1 Binary Size Constraints
* **Target Size:** The compiled `.clap` plugin binary must remain as small as possible — target under **100 kB**, with an absolute upper limit of **1 MB**.
* **Zero External Dependencies:** No heavy third-party GUI or DSP libraries (e.g. JUCE, Qt, Boost). Only standard C++ library headers and the lightweight CLAP API headers are permitted.
* **OS Native API Calls:** OS-specific native calls (such as Windows GDI/USER32/COM or X11/Cocoa) are acceptable for UI windowing and OS events.

### 1.2 UI Rendering & Performance Strategy
* **Procedural Graphics Engine:** All UI elements (knobs, switches, indicators, panels, borders, rivets) must be procedurally generated via pure code (or software raytracing/vector math). No heavy embedded bitmap textures.
* **Layered Abstraction Architecture:** The UI system must separate generic drawing primitives (line, rectangle, circle, arc, polygon, fill, gradient) from platform-specific backends (X11, Win32 GDI, Cocoa).
* **Layer Pre-Rendering & Rotation Cache:**
  - Complex UI controls (e.g. metallic knurled knobs, subtle drop shadows, dial indicators) should be pre-rendered into offscreen buffer layers during initialization or resized events.
  - Fast rotation, masking, and blitting operations are performed on pre-rendered layers at frame draw time to guarantee zero UI lag during real-time automation.
* **Panel & Border UI Components:** UI controls must be framed within distinct modular panels featuring metallic borders, dark recessed backgrounds, and industrial rivet/bezel decorations.

### 1.3 Simple Preset Management
* **Lightweight Format:** Minimal, human-readable preset serialization format (e.g., binary struct or lightweight key-value string stream). No heavy JSON/XML parsers.
* **Factory Presets:** Standard built-in bank of factory presets (e.g., *01: Hellfire Bass*, *02: Screaming Acid*, *03: Industrial Lead*, *04: Dark Pad*).

---

## 2. Implementation Roadmap

### Phase 1: Core Bootstrap & Architecture Setup (Completed)
- [x] Rename codebase from Syrebas to Gritbaal across all files, namespaces, header guards, CMake targets, and test suites.
- [x] Update project `README.md` with hero image `gritbaal_hero.jpg` and build instructions.
- [x] Establish `docs/design_document.md` and `docs/implementation_roadmap.md`.

### Phase 2: Core DSP Engine Expansion
- [ ] **Dual PolyBLEP VCOs:** Implement PolyBLEP anti-aliased saw, triangle, and pulse wave generation with variable pulse width (`src/core/Oscillator.cpp`).
- [ ] **Sync & FM:** Add hard synchronization of VCO2 to VCO1 and exponential cross-frequency modulation (FM).
- [ ] **Thermal Drift & Voice Allocation:** Add per-voice thermal pitch walk (1/f noise process) and initial component mismatch tolerances.
- [x] **Dual Filter Topology (VCF):** Non-linear ZDF 4-pole transistor ladder filter and Sallen-Key diode filter, both running inside a real 16-tap polyphase FIR 4x-oversampling loop with Newton-Raphson feedback solves and a Nyquist-aware cutoff clamp, with pre-filter drive (`src/core/Filter.cpp`).
- [x] **Overdrive & Power Sag:** Post-filter tube/diode asymmetric saturation and dynamic power rail sag under heavy bass transients (`src/core/SynthEngine.cpp`).
- [ ] **Oversample the post-filter waveshapers:** `warmthAmount`/`overdriveAmount` in `SynthEngine.cpp` currently run at the base sample rate after the filter's internal downsampling — route them through the filter's existing 4x FIR buffers instead. See [filter-architecture-plan.md](filter-architecture-plan.md) §1/§4.
- [x] **Per-synth VCF character presets (mono targets):** `Filter.hpp` now has a templated Saturator/Solver/Topology architecture (`TanhSaturator`/`AsymmetricSaturator`, `NewtonSolver`/`FixedPointSolver`, `LadderFilter`/`SallenKeyFilter`) with a `VintageFilterModel` selector wired up for Minimoog, ARP 2600 (4012), TB-303, and MS-20, each with its own per-stage capacitor mismatch and saturator family. Selected in the GUI via a drag-through "VCF MODEL" selector replacing the old binary Ladder/MS20 switch. Jupiter-8/CS-80 intentionally deferred (polyphonic). Remaining calibration gaps (ARP 2600 vs. Minimoog tuning, resonance-gain constants) tracked in [filter-architecture-plan.md](filter-architecture-plan.md).

### Phase 3: Procedural UI Engine & Knobs Layering
- [ ] **Layer Pre-Renderer:** Implement offscreen image buffer layer caching in `src/gui/` for pre-rendering knobs, dial indicators, shadows, and panel backgrounds.
- [ ] **Panel Frames & Borders:** Create modular procedural panel rendering components (dark steel background, copper trim, rivet accents).
- [ ] **Procedural Controls:** Render knurled metal knobs, toggle switches, and volcanic LED indicators using procedural math.

### Phase 4: Preset Management & CLAP State Handling
- [ ] **Preset Serializer:** Implement lightweight C++ binary/text state stream save/load (`GritbaalClap::stateSave`, `GritbaalClap::stateLoad`).
- [ ] **Factory Preset Bank:** Embed 8-16 curated factory presets into `GritbaalClap`.

### Phase 5: Verification & Optimization
- [ ] Validate plugin binary size remains strictly under 1 MB (target < 100 kB).
- [ ] Run DSP and GUI test executables (`gritbaal_dsp_test` and `gritbaal_gui_test`) to verify real-time performance, audio quality, and zero memory leaks.

---

*Document prepared for Gritbaal Synth project strategy.*
