add_executable(DeviceAccessModule 
    main.cpp 
    Gb28181Client.cpp 
    WebServer.cpp
)

target_link_libraries(DeviceAccessModule PRIVATE ${OSIP2_LIBRARIES} ${LIBXML2_LIBRARIES} pthread)
