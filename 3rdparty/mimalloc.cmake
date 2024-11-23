if(SCORE_DISABLE_SNMALLOC)
  return()
endif()

if(EMSCRIPTEN)
  return()
endif()

if(SCORE_HAS_SANITIZERS)
  return()
endif()

if(SCORE_USE_SYSTEM_LIBRARIES)
  find_package(snmalloc GLOBAL CONFIG)
else()
  block()
    set(SNMALLOC_BUILD_TESTING OFF CACHE INTERNAL "" FORCE)
    add_subdirectory(3rdparty/snmalloc SYSTEM)
  endblock()
endif()
