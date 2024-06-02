list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/modules")

include(BuildType)
include(BuildLocation)

include(AddFFMpeg)
include(AddBoost)
include(AddLibV4l2)
