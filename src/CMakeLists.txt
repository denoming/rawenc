include(GNUInstallDirs)

set(TARGET RawEnc)

add_executable(${TARGET} "")
add_executable(RawEnc::RawEnc ALIAS ${TARGET})

set_target_properties(${TARGET}
    PROPERTIES
        OUTPUT_NAME rawenc
)

target_sources(${TARGET}
    PRIVATE Application.cpp
            Camera.cpp
            Encoder.cpp
            LoggerInitializer.cpp
)

target_link_libraries(${TARGET}
    PRIVATE Boost::headers
            Boost::program_options
            LibV4l2::LibV4l2
            spdlog::spdlog
            PkgConfig::LibAvCodec
            PkgConfig::LibSigCpp
)

target_compile_features(${TARGET} PRIVATE cxx_std_20)

install(
    TARGETS ${TARGET}
    COMPONENT RawEnc_Runtime
)
