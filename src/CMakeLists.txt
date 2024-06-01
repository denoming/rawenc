include(GNUInstallDirs)

set(TARGET RawEnc)

add_executable(${TARGET} "")
add_executable(RawEnc::RawEnc ALIAS ${TARGET})

set_target_properties(${TARGET}
    PROPERTIES
        OUTPUT_NAME rawenc
)

target_sources(${TARGET}
    PRIVATE
        main.cpp
)

target_compile_features(${TARGET} PRIVATE cxx_std_17)

install(
    TARGETS ${TARGET}
    COMPONENT RAWENC_Runtime
)
