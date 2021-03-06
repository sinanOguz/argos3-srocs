if(ARGOS_BUILD_FOR_SIMULATOR)
  # build the qtopengl user functions
  if(ARGOS_COMPILE_QTOPENGL)
    add_library(builderbot_qtopengl_user_functions MODULE
      simulator/qt-opengl/builderbot_qtopengl_user_functions.h
      simulator/qt-opengl/builderbot_qtopengl_user_functions_widget.h
      simulator/qt-opengl/builderbot_qtopengl_user_functions.cpp
      simulator/qt-opengl/builderbot_qtopengl_user_functions_widget.cpp)
    target_link_libraries(builderbot_qtopengl_user_functions
      argos3srocs_${ARGOS_BUILD_FOR}_entities
      argos3srocs_${ARGOS_BUILD_FOR}_builderbot
      ${ARGOS_QTOPENGL_LIBRARY})
  endif(ARGOS_COMPILE_QTOPENGL)
  # configure and install the builderbot simulation tests
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/test_builderbot.argos.in
    ${CMAKE_BINARY_DIR}/testing/builderbot/test_builderbot.argos)
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/test_builderbot.lua.in
    ${CMAKE_BINARY_DIR}/testing/builderbot/test_builderbot.lua)
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/test_sample_rules.lua
    ${CMAKE_BINARY_DIR}/testing/builderbot/sample_rules.lua
    COPYONLY)   
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/test_block.lua
    ${CMAKE_BINARY_DIR}/testing/builderbot/test_block.lua
    COPYONLY)
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/example_builderbot_basic.argos.in
    ${CMAKE_BINARY_DIR}/testing/builderbot/example_builderbot_basic.argos)
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/example_builderbot_basic.lua.in
    ${CMAKE_BINARY_DIR}/testing/builderbot/example_builderbot_basic.lua)
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/example_builderbot_advance.argos.in
    ${CMAKE_BINARY_DIR}/testing/builderbot/example_builderbot_advance.argos)
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/example_builderbot_advance.lua.in
    ${CMAKE_BINARY_DIR}/testing/builderbot/example_builderbot_advance.lua)
  configure_file(
    ${CMAKE_SOURCE_DIR}/testing/builderbot/simulator/example_block.lua
    ${CMAKE_BINARY_DIR}/testing/builderbot/example_block.lua
    COPYONLY)
elseif(ARGOS_BUILD_FOR STREQUAL "builderbot")
   set(ARGOS_TEST_DIR $ENV{HOME}/testing)
   message("No tests added for the builderbot configuration")
endif(ARGOS_BUILD_FOR_SIMULATOR)
