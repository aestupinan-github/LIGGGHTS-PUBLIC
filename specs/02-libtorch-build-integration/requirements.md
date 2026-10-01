# Spec 02 — LibTorch Build Integration

## Requirements

### Clean CMake Integration
- User-configurable LibTorch path (environment variable or CMake variable)
- `find_package(Torch)` with proper paths
- Must not break `SCAN_STYLES()` or `style_contact_model.h` generation
- No hardcoded paths

### Clean Makefile Integration
- Same approach as CMake — user-configurable path
- No hardcoded paths to user directories
- Must work with legacy `src/MAKE/` build system

### Model Storage
- Folder structure: `SS_models/<shape>-<object>/overlap_model.pt` + `scalers.dat`
- Example: `SS_models/ss-cube-walls/overlap_model.pt` + `scalers.dat`
- Model loading at `init()` (not per-contact)

### Portability
- Must work across different systems without code changes
- LibTorch path configurable via environment variable or CMake/Makefile variable
