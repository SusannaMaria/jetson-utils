
file(GLOB gstCameraSources *.cpp)
file(GLOB gstCameraIncludes *.h )

add_executable(gst-pipeline ${gstCameraSources})
target_link_libraries(gst-pipeline jetson-utils)

install(TARGETS gst-pipeline DESTINATION bin)
