FUNCTION(SETUP_TARGET_FOR_COVERAGE path targetname)

  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  IF(NOT FASTCOV)
    MESSAGE(FATAL_ERROR "fastcov not found! Aborting...")
  ENDIF()

  IF(NOT GENHTML)
    MESSAGE(FATAL_ERROR "genhtml not found! Aborting...")
  ENDIF()

  IF(NOT SED)
    MESSAGE(FATAL_ERROR "sed not found! Aborting...")
  ENDIF()

  # Set shorcut for output: path/target
  set(OUTPUT ${path}/${targetname})
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${path})

  # Setup code coverage target
  ADD_CUSTOM_TARGET(${targetname}
    # Zero coverage counters
    COMMAND ${FASTCOV} --zerocounters
    # Run regression tests
    COMMAND ${CMAKE_CTEST_COMMAND} -j${PROCESSOR_COUNT}
    # Process gcov output for genhtml
    COMMAND ${FASTCOV} --branch-coverage --exceptional-branch-coverage --lcov -o ${OUTPUT}.info --exclude test/ c++/ include/ boost/
    # Copy over report customization files for genhtml
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/doc/piac.gcov.css
            ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/doc/piac.lcov.prolog
            ${CMAKE_BINARY_DIR}
    # Generate HTML report
    COMMAND ${GENHTML} --legend --rc genhtml_branch_coverage=1 --demangle-cpp --css-file piac.gcov.css --ignore-errors source --html-prolog piac.lcov.prolog --title "Piac-${CMAKE_BUILD_TYPE}" -o ${OUTPUT} ${OUTPUT}.info
    # Customize page headers in generated html to own
    COMMAND find ${OUTPUT} -type f -exec ${SED} -i "s/LCOV - code coverage report/Piac test code coverage report/g" {} +
    COMMAND find ${OUTPUT} -type f -exec ${SED} -i "s/<td class=\"headerItem\">Test:<\\/td>/<td class=\"headerItem\">Commit:<\\/td>/g" {} +
    # Cleanup intermediate data
    COMMAND ${CMAKE_COMMAND} -E remove ${OUTPUT}.info
    # Set work directory for target
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    # Echo what is being done
    COMMENT "Test code coverage report"
    VERBATIM USES_TERMINAL
  )

  # Make test coverage target dependent on optional dependencies passed in using
  # keyword DEPENDS
  add_dependencies(${targetname} ${ARG_DEPENDS})

  # Output code coverage target enabled
  message(STATUS "Enabling code coverage target '${targetname}' generating test coverage, dependencies {${ARG_DEPENDS}}, report at ${OUTPUT}/index.html")

ENDFUNCTION()
