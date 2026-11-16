# SHOBEATS SOUND LAB generator: offline, deterministic, never part of the audio callback.
#   shobeats_sound_lab generate --out presets/lab [--reference <dir>] [--exclude <curation.json>]
#                               [--include-rejected] [--batches 5] [--per-category 10] [--seed 2026]
add_executable(shobeats_sound_lab Main.cpp)
target_link_libraries(shobeats_sound_lab PRIVATE ${PRODUCT_NAME})
target_include_directories(shobeats_sound_lab PRIVATE $<TARGET_PROPERTY:${PRODUCT_NAME},INCLUDE_DIRECTORIES>)
target_compile_definitions(shobeats_sound_lab PRIVATE $<TARGET_PROPERTY:${PRODUCT_NAME},COMPILE_DEFINITIONS>)
