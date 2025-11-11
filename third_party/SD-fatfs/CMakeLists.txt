add_library(SD-fatfs INTERFACE)


target_sources(SD-fatfs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/fatfs/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/fatfs/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/fatfs/source/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/SD/sd_spi.c
)
target_include_directories(SD-fatfs INTERFACE
    fatfs/source
    SD
)

target_link_libraries(SD-fatfs INTERFACE
    hardware_pio
    hardware_spi
    pico_stdlib
)
