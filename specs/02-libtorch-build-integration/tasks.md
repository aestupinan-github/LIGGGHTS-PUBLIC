# Spec 02 — LibTorch Build Integration: Tasks

**Status:** Design-approved (implementation pending Phase 3D)

## Critical Implementation Tasks (Link to Phase 3D)

- [ ] **T2.1: CMake LibTorch integration** 
  - Add LIBTORCH_DIR variable and find_package(Torch) logic to src/CMakeLists.txt
  - Add error message with suggestions if Torch not found
  - Verify no hardcoded paths in CMake
  - Done when: CMake build passes with -DLIBTORCH_DIR and links correctly

- [ ] **T2.2: Model path resolution at runtime**
  - Implement resolve_model_path() following fallback algorithm (Design section)
  - Implement resolve_scaler_path() following same algorithm
  - Add error messages with full paths and suggestions
  - Done when: Runtime correctly resolves models and errors are diagnostic

- [ ] **T2.3: Model and scaler loading at init()**
  - Call torch::jit::load() with error handling
  - Call scalers parser (from Spec 00) with error handling
  - Store loaded model and scalers in global/class state
  - Done when: init() successfully loads both model and scalers

- [ ] **T2.4: Makefile integration** (Legacy build support)
  - Create Makefile.user template with LIBTORCH_DIR and linker flags
  - Document in BUILDING.md or similar
  - Test that legacy build system can use Makefile.user
  - Done when: Legacy make -f Makefile.serial builds with LibTorch

- [ ] **T2.5: Portable error messages**
  - All error messages include full paths attempted
  - All error messages include actionable suggestions
  - Errors use LIGGGHTS error->all(FLERR, ...) pattern
  - Done when: Users see helpful messages instead of cryptic errors

## Testing Tasks (Link to Spec 05)

- [ ] **T2.6: Unit test for path resolution**
  - Mock filesystem; test fallback order
  - Test SS_MODELS_DIR environment variable override
  - Done when: Path resolution is verified for all search locations

- [ ] **T2.7: Integration test for model loading**
  - Load run_029 and run_030 models successfully
  - Verify model type detection (option4 vs option1)
  - Verify inference runs without error
  - Done when: Both models load and forward pass succeeds

- [ ] **T2.8: Error handling test**
  - Test missing model file → diagnostic error
  - Test corrupted scalers.dat → diagnostic error
  - Test inference with invalid input → diagnostic error
  - Done when: All error paths produce actionable messages

## Documentation Tasks

- [ ] **T2.9: BUILDING.md documentation**
  - Document CMake build with LIBTORCH_DIR
  - Document Makefile.user setup
  - Document model staging (SS_models/ directory)
  - Example: development vs. system installation
  - Done when: Users can build LIGGGHTS with surrogates by following docs

- [ ] **T2.10: Deployment guide**
  - Docker example
  - Container example
  - System installation instructions
  - Development workflow
  - Done when: Multiple deployment workflows are documented
