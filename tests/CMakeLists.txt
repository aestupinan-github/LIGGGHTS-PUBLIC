# CTest test suite (Spec 05). Included from src/CMakeLists.txt; REPO = repository root.
#   ctest --test-dir build --output-on-failure
# Surrogate tests are registered only when ENABLE_SURROGATE=ON (RF-22).

SET(REPO ${CMAKE_CURRENT_SOURCE_DIR}/..)
SET(SURR_CASES ${REPO}/test_cases/surrogate_contact)
FIND_PACKAGE(Python3 COMPONENTS Interpreter)

# ---- sphere regression (always): a standard tutorial runs to completion (RF-14, RF-21)
ADD_TEST(NAME sphere_cohesion_tutorial_runs
         COMMAND $<TARGET_FILE:liggghts_bin> -in in.cohesion
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test_work/cohesion)
FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_work/cohesion/post)
FILE(COPY ${REPO}/examples/LIGGGHTS/Tutorials_public/cohesion/ DESTINATION ${CMAKE_BINARY_DIR}/test_work/cohesion)
SET_TESTS_PROPERTIES(sphere_cohesion_tutorial_runs PROPERTIES
  FAIL_REGULAR_EXPRESSION "ERROR"  TIMEOUT 600  LABELS "regression")

IF(ENABLE_SURROGATE)
  SET(SS_ENV "SS_MODELS_DIR=${REPO}/SS_models;OMP_NUM_THREADS=1")

  # ---- unit tests (RF-2, RF-15..18)
  ADD_EXECUTABLE(test_surrogate_geometry unit/test_surrogate_geometry.cpp)
  TARGET_INCLUDE_DIRECTORIES(test_surrogate_geometry PRIVATE ${REPO}/src unit)
  ADD_TEST(NAME surrogate_geometry COMMAND test_surrogate_geometry)

  ADD_EXECUTABLE(test_surrogate_model unit/test_surrogate_model.cpp ${REPO}/src/scaler_parser.cpp)
  TARGET_INCLUDE_DIRECTORIES(test_surrogate_model PRIVATE ${REPO}/src unit ${TORCH_INCLUDE_DIRS})
  TARGET_LINK_LIBRARIES(test_surrogate_model PRIVATE torch torch_cpu c10)
  ADD_TEST(NAME surrogate_model COMMAND test_surrogate_model ${REPO}/SS_models/ss-cube-walls)
  SET_TESTS_PROPERTIES(surrogate_geometry surrogate_model PROPERTIES LABELS "unit;surrogate")

  # ---- regression tests vs frozen reference results (RF-19)
  IF(Python3_FOUND)
    FOREACH(CASE test_2:test_02_cube_1cm_falling
                 test_03_cube_1cm_corner:test_03_cube_1cm_corner
                 test_03b_cube_1cm_corner_tilt25:test_03b_cube_1cm_corner_tilt25
                 test_04_cube_1cm_edge:test_04_cube_1cm_edge
                 test_05c_cube_1cm_spin_h010:test_05c_cube_1cm_spin_h010)
      STRING(REPLACE ":" ";" P ${CASE})
      LIST(GET P 0 REFDIR)
      LIST(GET P 1 INPUT)
      ADD_TEST(NAME surrogate_regression_${INPUT}
               COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/regression/check_case.py
                       $<TARGET_FILE:liggghts_bin> ${SURR_CASES}/${INPUT}.in
                       ${SURR_CASES}/reference/${REFDIR}/output.log 1e-6)
      SET_TESTS_PROPERTIES(surrogate_regression_${INPUT} PROPERTIES
        ENVIRONMENT "${SS_ENV}" TIMEOUT 900 LABELS "regression;surrogate")
    ENDFOREACH()

    # multi-particle (Spec 06): 125 cubes, 6 walls, batched model calls; serial vs reference
    ADD_TEST(NAME surrogate_regression_pour_125_cubes
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/regression/check_case.py
                     $<TARGET_FILE:liggghts_bin> ${REPO}/test_cases/surrogate_multi/in.pour_125_cubes
                     ${REPO}/test_cases/surrogate_multi/reference/np1/output.log 1e-6)
    SET_TESTS_PROPERTIES(surrogate_regression_pour_125_cubes PROPERTIES
      ENVIRONMENT "${SS_ENV}" TIMEOUT 900 LABELS "regression;surrogate;multi")
  ELSE()
    MESSAGE(STATUS "Python3 not found: surrogate regression tests not registered")
  ENDIF()

  # ---- error handling (RF-20): clean LIGGGHTS error message, process exits non-zero.
  # Error::one() calls MPI_Abort without flushing stdout; when stdout is a pipe (CTest) the
  # message would be lost -> run unbuffered with stdbuf (upstream LIGGGHTS behaviour, not changed).
  FIND_PROGRAM(STDBUF stdbuf)
  ADD_TEST(NAME surrogate_error_unknown_shape
           COMMAND ${STDBUF} -o0 -e0 $<TARGET_FILE:liggghts_bin> -in ${CMAKE_CURRENT_SOURCE_DIR}/regression/in.error_unknown_shape
           WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  ADD_TEST(NAME surrogate_error_surface_needs_surrogate
           COMMAND ${STDBUF} -o0 -e0 $<TARGET_FILE:liggghts_bin> -in ${CMAKE_CURRENT_SOURCE_DIR}/regression/in.error_surface_needs_surrogate
           WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  SET_TESTS_PROPERTIES(surrogate_error_unknown_shape PROPERTIES
    ENVIRONMENT "${SS_ENV}" PASS_REGULAR_EXPRESSION "ERROR.*ss-does-not-exist" LABELS "surrogate")
  SET_TESTS_PROPERTIES(surrogate_error_surface_needs_surrogate PROPERTIES
    ENVIRONMENT "${SS_ENV}" PASS_REGULAR_EXPRESSION "ERROR.*requires atom_style surrogate" LABELS "surrogate")
ENDIF()
