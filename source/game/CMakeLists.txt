aux_source_directory(. DIR_SRCS)
add_library(InnoGame SHARED ${DIR_SRCS})
include (GenerateExportHeader)
GENERATE_EXPORT_HEADER (InnoGame
    BASE_NAME INNO_GAME
    EXPORT_MACRO_NAME INNO_GAME_EXPORT
    EXPORT_FILE_NAME ${CMAKE_SOURCE_DIR}/game/exports/InnoGame_Export.h
    STATIC_DEFINE INNO_GAME_BUILT_AS_STATIC
)

link_directories(${CMAKE_SOURCE_DIR}/engine/system/)
target_link_libraries(InnoGame InnoSystem)
