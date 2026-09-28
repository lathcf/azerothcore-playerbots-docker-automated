if(TARGET modules)
  # This module's own headers. mod-ahbot-price (AHPriceCalc.h), mod-individual-progression and
  # mod-era-talents headers are already on the shared `modules` target's include path.
  target_include_directories(modules PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
endif()
