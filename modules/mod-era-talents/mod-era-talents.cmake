if(TARGET modules)
  target_include_directories(modules PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
  # mod-individual-progression header (sIndividualProgression, ProgressionState) for EraTalentIP.cpp.
  set(_IP_SRC "${CMAKE_CURRENT_LIST_DIR}/../mod-individual-progression/src")
  if(EXISTS "${_IP_SRC}")
    target_include_directories(modules PRIVATE ${_IP_SRC})
    message(STATUS "[mod-era-talents] mod-individual-progression headers on include path")
  else()
    message(WARNING "[mod-era-talents] mod-individual-progression not found; build will fail until it is cloned")
  endif()
  # mod-playerbots headers (GET_PLAYERBOT_AI) for the bot-skip guard in EraTalentPin.cpp.
  # NB: never include IP's header in the SAME TU as playerbots' (GENERAL enum clash) — EraTalentIP.cpp
  # is the only file that includes IP, and it does NOT include playerbots; the pin TU is the reverse.
  set(_PB_SRC "${CMAKE_CURRENT_LIST_DIR}/../mod-playerbots/src")
  if(EXISTS "${_PB_SRC}")
    target_include_directories(modules PRIVATE ${_PB_SRC} ${_PB_SRC}/Script ${_PB_SRC}/Bot ${_PB_SRC}/Ai/Base)
    message(STATUS "[mod-era-talents] mod-playerbots headers on include path")
  else()
    message(WARNING "[mod-era-talents] mod-playerbots not found; build will fail until it is cloned")
  endif()
endif()
