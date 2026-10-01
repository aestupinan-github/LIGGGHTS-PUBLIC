# Spec 02 — LibTorch Build Integration: Design

## CMake Integration

```cmake
# User sets LIBTORCH_DIR environment variable or passes -DLIBTORCH_DIR=<path>
IF(NOT DEFINED LIBTORCH_DIR)
  SET(LIBTORCH_DIR $ENV{LIBTORCH_DIR})
ENDIF()

IF(LIBTORCH_DIR)
  LIST(APPEND CMAKE_PREFIX_PATH "${LIBTORCH_DIR}")
  FIND_PACKAGE(Torch REQUIRED)
  # Link against liggghts targets
ENDIF()
```

## Makefile Integration

```makefile
# In Makefile.user or Makefile.user_default:
LIBTORCH_DIR ?= $(HOME)/libtorch
EXTRA_LIB += -L$(LIBTORCH_DIR)/lib -ltorch -ltorch_cpu -lc10
EXTRA_INC += -I$(LIBTORCH_DIR)/include -I$(LIBTORCH_DIR)/include/torch/csrc/api/include
```

## Model Storage

```
SS_models/
  ss-cube-walls/
    overlap_model.pt
    scalers.dat
  ss-piramide-walls/
    overlap_model.pt
    scalers.dat
  ss-cube-ss-cube/
    overlap_model.pt
    scalers.dat
```

## Model Loading

- Loaded once at `init()` (not per-contact)
- Model path resolved from shape name + object type
- Error if model file not found at init (fail fast)
