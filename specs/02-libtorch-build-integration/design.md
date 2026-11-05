# Spec 02 — LibTorch Build Integration: Design

**Status:** Approved (QA-resolved 2026-11-05)

## CMake Integration

### Build-Time Configuration

**User provides one of:**
```bash
# Option A: Environment variable
export LIBTORCH_DIR=/opt/libtorch
cmake -S . -B build

# Option B: CMake variable
cmake -S . -B build -DLIBTORCH_DIR=/opt/libtorch

# Option C: System-wide installation (auto-discovery)
cmake -S . -B build
# (find_package(Torch) searches standard paths)
```

### CMakeLists.txt Logic

```cmake
# In src/CMakeLists.txt (after LIGGGHTS is set up):

if(NOT DEFINED LIBTORCH_DIR)
    set(LIBTORCH_DIR $ENV{LIBTORCH_DIR})
endif()

if(LIBTORCH_DIR)
    message(STATUS "Using LIBTORCH_DIR: ${LIBTORCH_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${LIBTORCH_DIR}")
endif()

find_package(Torch REQUIRED)
message(STATUS "Found Torch: ${Torch_DIR}")

# Link to main library target
target_link_libraries(liggghts_obj PUBLIC torch torch_cpu c10)
target_include_directories(liggghts_obj PUBLIC ${TORCH_INCLUDE_DIRS})
```

### Error Handling

```cmake
if(NOT Torch_FOUND)
    message(FATAL_ERROR
        "LibTorch not found!\n"
        "Set LIBTORCH_DIR environment variable or pass -DLIBTORCH_DIR=<path> to cmake\n"
        "Example: cmake -DLIBTORCH_DIR=/opt/libtorch -S . -B build"
    )
endif()
```

---

## Makefile Integration (Legacy)

### Makefile.user Template

```makefile
# Optional: User-provided LibTorch configuration
# This file is created by user; not committed to repo

# Set LIBTORCH_DIR to your LibTorch installation
LIBTORCH_DIR ?= $(HOME)/libtorch

# Add LibTorch includes
EXTRA_INC += -I$(LIBTORCH_DIR)/include
EXTRA_INC += -I$(LIBTORCH_DIR)/include/torch/csrc/api/include

# Add LibTorch libraries
EXTRA_LIB += -L$(LIBTORCH_DIR)/lib
EXTRA_LIB += -ltorch -ltorch_cpu -lc10
```

### Build Instructions

```bash
# Step 1: Create Makefile.user if not present
cat > src/MAKE/Makefile.user << EOF
LIBTORCH_DIR = /opt/libtorch
EXTRA_INC += -I$(LIBTORCH_DIR)/include
EXTRA_INC += -I$(LIBTORCH_DIR)/include/torch/csrc/api/include
EXTRA_LIB += -L$(LIBTORCH_DIR)/lib
EXTRA_LIB += -ltorch -ltorch_cpu -lc10
EOF

# Step 2: Build with legacy makefile system
cd src && make -f Makefile.serial
```

---

## Model Storage and Discovery

### Directory Structure

```
LIGGGHTS/
  SS_models/
    ss-cube-walls/
      overlap_model.pt          (TorchScript model, run_029 or run_030)
      scalers.dat               (key-value metadata, format v1)
      
    ss-pyramid-walls/           (future interaction)
      overlap_model.pt
      scalers.dat
      
    ss-cube-ss-cube/            (future ss-ss contact)
      overlap_model.pt
      scalers.dat
```

### Runtime Path Resolution Algorithm

**At `init()` time, when shape ss-cube-walls is loaded:**

```cpp
std::string resolve_model_path(const std::string& shape_name) {
    // Step 1: Try environment variable SS_MODELS_DIR
    const char* env_dir = std::getenv("SS_MODELS_DIR");
    if (env_dir) {
        std::string path = std::string(env_dir) + "/" + shape_name + "-walls/overlap_model.pt";
        if (file_exists(path)) return path;
    }
    
    // Step 2: Try relative to executable: ./SS_models/
    std::string rel_path = "./SS_models/" + shape_name + "-walls/overlap_model.pt";
    if (file_exists(rel_path)) return rel_path;
    
    // Step 3: Try relative to source: ../SS_models/
    std::string src_rel = "../SS_models/" + shape_name + "-walls/overlap_model.pt";
    if (file_exists(src_rel)) return src_rel;
    
    // Step 4: Try system install: /usr/local/liggghts/SS_models/
    std::string sys_path = "/usr/local/liggghts/SS_models/" + shape_name + "-walls/overlap_model.pt";
    if (file_exists(sys_path)) return sys_path;
    
    // All failed: error with diagnostic
    error->all(FLERR, 
        fmt::format("Model not found for shape {}\n"
                   "Searched:\n"
                   "  1. SS_MODELS_DIR env var: {}\n"
                   "  2. Relative (./SS_models/): not found\n"
                   "  3. Source relative (../SS_models/): not found\n"
                   "  4. System (/usr/local/...): not found\n"
                   "Set SS_MODELS_DIR or place SS_models/ in build directory",
                   shape_name,
                   env_dir ? env_dir : "(not set)"));
    return "";  // unreachable
}
```

### Scaler File Discovery

Same algorithm, append `scalers.dat` instead of `overlap_model.pt`.

---

## Model Loading and Error Handling

### TorchScript Load

```cpp
try {
    auto model = torch::jit::load(model_path);
    // Success
} catch (const std::exception& e) {
    error->all(FLERR,
        fmt::format("TorchScript model load failed:\n"
                   "Path: {}\n"
                   "Error: {}",
                   model_path,
                   e.what()));
}
```

### Scaler File Parse

```cpp
try {
    parse_scalers(scaler_path);  // Custom parser from Spec 00
    // Success
} catch (const std::exception& e) {
    error->all(FLERR,
        fmt::format("Scaler file parse error:\n"
                   "Path: {}\n"
                   "Error: {}",
                   scaler_path,
                   e.what()));
}
```

### Model Inference

```cpp
try {
    auto output = model->forward(input_tensor);
    // Success
} catch (const std::exception& e) {
    error->all(FLERR,
        fmt::format("Model inference failed:\n"
                   "Input shape: {}\n"
                   "Input values: z_unit={}, q=[{},{},{},{}]\n"
                   "Error: {}",
                   input_shape,
                   z_unit, qw, qx, qy, qz,
                   e.what()));
}
```

---

## Deployment Workflows

### Development Workflow

```bash
# 1. Clone/build LIGGGHTS
git clone https://github.com/CFDEMcoupling/LIGGGHTS_PUBLIC.git
cd LIGGGHTS_PUBLIC

# 2. Copy trained models from training repo
mkdir -p SS_models/ss-cube-walls
cp /path/to/SS_models_training/interactions/ss_cube-wall/output/run_029/* \
   SS_models/ss-cube-walls/

# 3. Build with CMake
mkdir build && cd build
cmake -S .. -B . -DLIBTORCH_DIR=/opt/libtorch
cmake --build . -j4

# 4. Run LIGGGHTS (auto-discovers ./SS_models/)
./liggghts -in ../examples/surrogate_cube.lmp
```

### System Installation Workflow

```bash
# 1. Build and install LIGGGHTS
cmake --install build --prefix /usr/local/liggghts

# 2. Install models to system location
mkdir -p /usr/local/liggghts/SS_models/ss-cube-walls
cp run_029/* /usr/local/liggghts/SS_models/ss-cube-walls/

# 3. Users run (auto-discovers /usr/local/...)
/usr/local/liggghts/bin/liggghts -in input.lmp
```

### Docker/Container Workflow

```dockerfile
# Dockerfile example
FROM ubuntu:22.04

# Install build tools and LibTorch
RUN apt-get update && apt-get install -y cmake g++ libopenmpi-dev
RUN wget https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.0.1%2Bcpu.zip
RUN unzip libtorch-*.zip -d /opt

# Build LIGGGHTS
COPY . /app/liggghts
WORKDIR /app/liggghts/build
RUN cmake -S .. -B . -DLIBTORCH_DIR=/opt/libtorch
RUN cmake --build . -j4

# Stage models
COPY models/ ./SS_models/

# Run
ENTRYPOINT ["/app/liggghts/build/liggghts"]
```

---

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Path resolution | Fallback search order | Flexible; supports dev/system/container workflows |
| Error strategy | Fail-fast with diagnostics | Prevents silent failures; helps users debug |
| Environment variable | SS_MODELS_DIR | Standard; overrides all other search paths |
| CMake strategy | find_package(Torch) | Standard CMake pattern; maintainable |
| Makefile strategy | User-created Makefile.user | Non-invasive; doesn't break existing makefiles |
| Portability | No hardcoded paths | Works across Linux/macOS/Windows |

---

## Build and Runtime Flow Diagram

```
Build Time:
  cmake -DLIBTORCH_DIR=...
    → finds LibTorch
    → links torch, torch_cpu, c10
    → produces LIGGGHTS binary

Runtime (./liggghts -in input.lmp):
  init() called
    → input file declares: atom_style surrogate ss-cube
    → resolve_model_path("ss-cube")
      → check env SS_MODELS_DIR
      → check ./SS_models/
      → check ../SS_models/
      → check /usr/local/...
    → found: ./SS_models/ss-cube-walls/overlap_model.pt
    → load TorchScript model
    → load scalers.dat
    → ready for contact calculations
    
  During simulation:
    contact_calc() called
    → query model with [z_unit, quat]
    → denormalize output
    → rescale by L_actual
    → return overlap to physics model
```
