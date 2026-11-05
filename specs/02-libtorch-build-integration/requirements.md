# Spec 02 — LibTorch Build Integration

**Status:** Approved (QA-resolved 2026-11-05)

## Context and Objective

Define how LIGGGHTS discovers, loads, and links LibTorch (for TorchScript model inference), and how it locates trained surrogate models at runtime. Path resolution must be platform-independent, user-configurable, and fail-fast with clear diagnostics.

## Requirements

### R02.1: CMake LibTorch Integration

**Build-Time Path Resolution:**

```cmake
# User provides LIBTORCH_DIR via:
# 1. Environment variable: export LIBTORCH_DIR=/path/to/libtorch
# 2. CMake variable: cmake -DLIBTORCH_DIR=/path/to/libtorch ...
# 3. Default (if not set): attempt system find_package(Torch)

if(NOT DEFINED LIBTORCH_DIR)
    set(LIBTORCH_DIR $ENV{LIBTORCH_DIR})
endif()

if(LIBTORCH_DIR)
    # User provided explicit path
    list(APPEND CMAKE_PREFIX_PATH "${LIBTORCH_DIR}")
    find_package(Torch REQUIRED)
    target_link_libraries(liggghts_obj PUBLIC torch torch_cpu c10)
else()
    # Attempt system-wide LibTorch discovery
    find_package(Torch)
    if(NOT Torch_FOUND)
        message(FATAL_ERROR "LibTorch not found. Set LIBTORCH_DIR environment variable or pass -DLIBTORCH_DIR=<path>")
    endif()
    target_link_libraries(liggghts_obj PUBLIC torch torch_cpu c10)
endif()
```

**Portability Rules:**
- No hardcoded paths (e.g., `/home/user/libtorch`)
- LIBTORCH_DIR must accept both absolute and relative paths
- Relative paths resolved relative to build directory
- Spaces in paths must be handled correctly (quoted)

**Error Handling:**
- IF LibTorch not found and no LIBTORCH_DIR provided, error with diagnostic: "LibTorch not found. Set LIBTORCH_DIR environment variable or pass -DLIBTORCH_DIR=<path> to cmake"
- IF LibTorch version mismatch detected (optional), warn with message suggesting LIBTORCH_DIR override

### R02.2: Makefile Integration (Legacy Build System)

**User Configuration:**

```makefile
# In Makefile.user or Makefile.user_default (created by user):
LIBTORCH_DIR ?= $(HOME)/libtorch

EXTRA_INC += -I$(LIBTORCH_DIR)/include
EXTRA_INC += -I$(LIBTORCH_DIR)/include/torch/csrc/api/include
EXTRA_LIB += -L$(LIBTORCH_DIR)/lib
EXTRA_LIB += -ltorch -ltorch_cpu -lc10
```

**Portability Rules:**
- Same rules as CMake (no hardcoded paths)
- Support environment variable fallback: `LIBTORCH_DIR ?= $(LIBTORCH_DIR)`
- Document in build documentation (e.g., `BUILDING.md` or cmake-generated config)

### R02.3: Model Storage and Runtime Path Resolution

**Model Folder Structure:**

```
LIGGGHTS_ROOT/
  SS_models/
    ss-cube-walls/
      overlap_model.pt
      scalers.dat
    ss-pyramid-walls/
      overlap_model.pt
      scalers.dat
```

**Runtime Path Resolution (during `init()`):**

1. **Step 1: Resolve SS_models root directory**
   - Try option A: Environment variable `SS_MODELS_DIR`
   - Try option B: Relative to executable: `./SS_models/`
   - Try option C: Relative to LIGGGHTS source: `../SS_models/`
   - Try option D: Absolute path `/usr/local/liggghts/SS_models/` (system install)
   - IF none found, error with diagnostic listing all searched locations

2. **Step 2: Resolve model path for specific shape**
   - Input file specifies shape: `atom_style surrogate ss-cube`
   - Construct path: `${SS_MODELS_DIR}/ss-cube-walls/overlap_model.pt`
   - Construct scaler path: `${SS_MODELS_DIR}/ss-cube-walls/scalers.dat`
   - IF model not found, error with diagnostic including full path attempted

3. **Step 3: Load model and scalers**
   - Load TorchScript model: `torch::jit::load(model_path)`
   - Load scalers: parse ASCII key-value file (Spec 00)
   - IF either fails, error with diagnostic (exception message + path + suggestion)

**Example Error Messages:**

```
Error: SS_models directory not found.
Searched:
  - Environment variable SS_MODELS_DIR: not set
  - Relative to executable: ./SS_models/ (not found)
  - Relative to source: ../SS_models/ (not found)
  - System path: /usr/local/liggghts/SS_models/ (not found)
Suggestion: Set SS_MODELS_DIR or place SS_models/ in build directory
```

```
Error: Model not found for shape ss-cube
Path attempted: /path/to/SS_models/ss-cube-walls/overlap_model.pt
Suggestion: Check shape name is correct; verify SS_models directory contains ss-cube-walls/
```

### R02.4: Deployment and Installation

**Development Workflow:**
```bash
# After training Phase 3B:
# 1. Copy models from training repo to LIGGGHTS repo
cp -r SS_models_training/interactions/ss_cube-wall/output/run_029 LIGGGHTS/SS_models/ss-cube-walls

# 2. Set environment or use default (./SS_models/)
export SS_MODELS_DIR=./SS_models

# 3. Run LIGGGHTS
./build/liggghts -in input_script.lmp
```

**System Installation Workflow:**
```bash
# After building:
# 1. Install models to system location
mkdir -p /usr/local/liggghts/SS_models/ss-cube-walls
cp run_029/model.pt /usr/local/liggghts/SS_models/ss-cube-walls/overlap_model.pt
cp run_029/scalers.dat /usr/local/liggghts/SS_models/ss-cube-walls/

# 2. LIGGGHTS auto-discovers from /usr/local/liggghts/SS_models/
./build/liggghts -in input_script.lmp
```

**Container/CI Deployment Workflow:**
```bash
# In Dockerfile or CI script:
COPY SS_models/ /app/liggghts/SS_models/
ENV SS_MODELS_DIR=/app/liggghts/SS_models
RUN cd /app/liggghts && ./build/liggghts -in test.lmp
```

### R02.5: Error Handling and Diagnostics

**Model Load Failures:**
- IF model file corrupted or wrong format, error with: "TorchScript model load failed: <torch exception> at <path>"
- IF scalers.dat malformed, error with: "Scaler file parse error: <detail> at <path>"
- IF scaler format version unsupported, error with: "Unsupported scaler format version: <version> (expected v1)"

**Verify Model-Scaler Pairing:**
- IF model.pt and scalers.dat do not match (optional checksum or version mismatch), warn with diagnostic
- Proceed anyway (acceptable; models may be manually paired)

**Model Inference Failures:**
- IF model.forward() throws exception, error with: "Model inference failed: <exception> with input <values>"
- Include input tensor shape, dtype, and values for diagnostics

### R02.6: Portability Across Platforms

**Supported Platforms:**
- Linux (primary): x86_64, ARM64
- macOS: x86_64, ARM64 (Apple Silicon)
- Windows: x86_64 (via WSL or native)

**LibTorch Versions:**
- Minimum: LibTorch 1.12 (CPU, CPU+CUDA optional)
- Tested: LibTorch 2.0+ (CPU)
- User must provide compatible LibTorch for their platform

**Executable Portability:**
- LIGGGHTS binary compiled with LibTorch is NOT portable across systems
- Recommend: Either (A) static linking, or (B) ship LibTorch libs with binary, or (C) users build locally with their LIBTORCH_DIR
- Document this limitation in BUILDING.md

---

## Definitions

| Term | Definition |
|------|-----------|
| **LIBTORCH_DIR** | Root directory of LibTorch installation (contains `lib/`, `include/`, `share/`) |
| **SS_MODELS_DIR** | Root directory of surrogate models (contains subdirs like `ss-cube-walls/`) |
| **Model Path** | Absolute path to `overlap_model.pt` for a specific shape |
| **Scaler Path** | Absolute path to `scalers.dat` for a specific shape |
| **Portability** | Code/build procedure works on different platforms without modification |

---

## Functional Requirements

- RF-1: THE SYSTEM SHALL accept LIBTORCH_DIR via environment variable or CMake variable
- RF-2: THE SYSTEM SHALL not hardcode any paths; all paths user-configurable
- RF-3: THE SYSTEM SHALL resolve SS_models directory at runtime using fallback search order
- RF-4: THE SYSTEM SHALL load TorchScript models and scaler files at `init()` time
- RF-5: WHEN model or scaler not found, THE SYSTEM SHALL error with diagnostic including full path and suggestions
- RF-6: THE SYSTEM SHALL support both development (./SS_models/) and system installation (/usr/local/...) workflows
- RF-7: THE SYSTEM SHALL work without modification on Linux, macOS, and Windows

---

## Non-Functional Requirements

- NFR-1: Path resolution must be fast (< 100ms for model loading at init time)
- NFR-2: Error messages must be actionable and include paths searched
- NFR-3: Build must not break if LibTorch is not installed (graceful error)
- NFR-4: Installation must be straightforward (copy SS_models/ folder, no special steps)

---

## Edge Cases

- EC-1: WHEN LIBTORCH_DIR contains spaces, THE SYSTEM SHALL handle correctly (quoted paths)
- EC-2: WHEN SS_MODELS_DIR is a relative path, THE SYSTEM SHALL resolve relative to current working directory
- EC-3: WHEN model.pt and scalers.dat have mismatched versions, THE SYSTEM MAY warn but SHALL proceed
- EC-4: WHEN multiple SS_models directories exist (env var + local + system), THE SYSTEM SHALL use first found (precedence order documented)

---

## Out of Scope

- Network download of models (models must be pre-staged)
- Automatic LibTorch download (users provide LIBTORCH_DIR)
- Per-particle model selection at runtime (future: Phase 3D)
- CUDA/GPU support (CPU-only for Phase 3C)

---

## Completion Criteria

- CC-1: CMake build accepts LIBTORCH_DIR and finds LibTorch without hardcoded paths
- CC-2: Makefile build accepts LIBTORCH_DIR and links correctly
- CC-3: Runtime path resolution follows documented fallback order
- CC-4: Error messages include full paths and actionable suggestions
- CC-5: Models can be deployed in development (./SS_models/) and system (/usr/local/) locations
- CC-6: Build works on Linux, macOS, Windows without modification
