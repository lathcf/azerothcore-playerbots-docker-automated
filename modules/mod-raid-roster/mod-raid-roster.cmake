if(TARGET modules)
  # This module's own headers.
  target_include_directories(modules PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

  # mod-playerbots headers (AddPlayerBot, PlayerbotFactory, addclassCache, spec-tab enums).
  # The fork compiles all modules into the single `modules` target, so these are usually already
  # on the include path; add them explicitly to be safe if mod-playerbots is present.
  set(_PB_SRC "${CMAKE_CURRENT_LIST_DIR}/../mod-playerbots/src")
  if(EXISTS "${_PB_SRC}")
    target_include_directories(modules PRIVATE
      ${_PB_SRC}
      ${_PB_SRC}/Bot
      ${_PB_SRC}/Bot/Factory
      ${_PB_SRC}/Script
      ${_PB_SRC}/Ai/Base)
    message(STATUS "[mod-raid-roster] mod-playerbots headers on include path")
  else()
    message(WARNING "[mod-raid-roster] mod-playerbots not found; build will fail until it is cloned")
  endif()
endif()
