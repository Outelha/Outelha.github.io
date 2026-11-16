# Command-line preset converter: same code as the plugin's "Import Presets" button.
#   shobeats_preset_import <zip | folder | .fwd/.fmod> <output folder> [--audio-check]
add_executable(shobeats_preset_import Main.cpp)
target_link_libraries(shobeats_preset_import PRIVATE ${PRODUCT_NAME})
target_include_directories(shobeats_preset_import PRIVATE $<TARGET_PROPERTY:${PRODUCT_NAME},INCLUDE_DIRECTORIES>)
target_compile_definitions(shobeats_preset_import PRIVATE $<TARGET_PROPERTY:${PRODUCT_NAME},COMPILE_DEFINITIONS>)
