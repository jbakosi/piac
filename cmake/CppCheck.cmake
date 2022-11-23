find_program( CPPCHECK cppcheck )
find_program( CPPCHECK_HTMLREPORT cppcheck-htmlreport )

if(CPPCHECK AND CPPCHECK_HTMLREPORT)

  find_program( FILEFIND find )
  find_program( SED sed )

  if(FILEFIND AND SED)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/doc/html/${CMAKE_BUILD_TYPE}/cppcheck)
    ADD_CUSTOM_TARGET(cppcheck-xml
      # Run cppcheck static analysis
      COMMAND ${CPPCHECK} --inline-suppr --enable=all --force
              --xml --xml-version=2 -j${PROCESSOR_COUNT}
              ${CMAKE_CURRENT_SOURCE_DIR}/src
              2> doc/html/${CMAKE_BUILD_TYPE}/cppcheck/cppcheck-report.xml
      # Generate html output
      COMMAND ${CPPCHECK_HTMLREPORT}
              --file=doc/html/${CMAKE_BUILD_TYPE}/cppcheck/cppcheck-report.xml
              --report-dir=doc/html/${CMAKE_BUILD_TYPE}/cppcheck --source-dir=.
      # Customize page headers in generated html
      COMMAND ${FILEFIND} doc/html/${CMAKE_BUILD_TYPE}/cppcheck -type f -exec ${SED} -i "s/project name/Piac-${CMAKE_BUILD_TYPE}/g" {} +
      # Set work directory for target
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      # Echo what is being done
      COMMENT "Cppcheck-xml static analysis report"
      VERBATIM USES_TERMINAL
    )
    # Output code coverage target enabled
    message(STATUS "Enabling cppcheck static analysis target 'cppcheck-xml', report at ${CMAKE_BINARY_DIR}/doc/html/${CMAKE_BUILD_TYPE}/cppcheck/index.html")
  endif()

endif()
