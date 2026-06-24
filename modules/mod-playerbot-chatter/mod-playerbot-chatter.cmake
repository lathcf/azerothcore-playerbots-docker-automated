if(TARGET modules)
  # nlohmann/json: prefer the bundled copy (this AC fork ships no nlohmann), then
  # AzerothCore deps, then a system package. The bundled header lives at
  # deps/nlohmann/json.hpp so PBChatterOllama.cpp can `#include <nlohmann/json.hpp>`.
  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/deps/nlohmann/json.hpp")
    target_include_directories(modules PRIVATE ${CMAKE_CURRENT_LIST_DIR}/deps)
    message(STATUS "[mod-playerbot-chatter] Using bundled nlohmann/json")
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/deps/nlohmann")
    target_include_directories(modules PRIVATE ${CMAKE_SOURCE_DIR}/deps/nlohmann)
    message(STATUS "[mod-playerbot-chatter] Using AzerothCore deps nlohmann/json")
  else()
    find_package(nlohmann_json CONFIG QUIET)
    if(nlohmann_json_FOUND)
      target_link_libraries(modules PRIVATE nlohmann_json::nlohmann_json)
      message(STATUS "[mod-playerbot-chatter] Using system nlohmann/json")
    else()
      message(FATAL_ERROR "[mod-playerbot-chatter] nlohmann/json not found")
    endif()
  endif()

  # cpp-httplib is header-only and vendored in src/.
  target_include_directories(modules PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

  find_package(Threads REQUIRED)
  target_link_libraries(modules PRIVATE Threads::Threads)
endif()
