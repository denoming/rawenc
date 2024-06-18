include(FeatureSummary)

option(RAWENC_ENABLE_NVCODEC "Enable Nvidia codec" OFF)
if(RAWENC_ENABLE_NVCODEC)
    list(APPEND VCPKG_MANIFEST_FEATURES "nvc")
endif()
add_feature_info(
    RAWENC_ENABLE_NVCODEC RAWENC_ENABLE_NVCODEC "Build project with Nvidia codec"
)

feature_summary(WHAT ALL)
