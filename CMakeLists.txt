cmake_minimum_required(VERSION 2.8.11)

if (CMAKE_VERSION VERSION_LESS "3.1")
    set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11")
else ()
   set (CMAKE_CXX_STANDARD 11)	
endif ()


project(dht)

FILE(GLOB DhtHeader
        core/dht_demoapi.h
        core/dht_head.h
        core/dht_helper.h
        core/dht_net.h
        core/dht_worker.h)

set(DhtSource core/dht_helper.cpp
			  core/dht_net.cpp
			  core/dht_worker.cpp
			  core/dht_demo.cpp)

#SET(DhtDataProtob protob/dht_data.pb.h protob/dht_data.pb.cc)

#SET(BroadcastSource broadcast/NeighborBroadcast.h
#		broadcast/NeighborBroadcast.cpp
#		broadcast/NeighborBroadcastImpl.h broadcast/NeighborBroadcastImpl.cpp)

INCLUDE_DIRECTORIES(core/ )
#INCLUDE_DIRECTORIES(core/ dpos/ protob/)
#INCLUDE_DIRECTORIES(include/protobuf_include/)
LINK_DIRECTORIES(lib/)

# Tell CMake to create the helloworld executable
add_library(dht-lib SHARED ${DhtHeader} ${DhtSource}
		${DhtDataProtob} )

if(WIN32)
        target_compile_definitions(dht-lib PRIVATE DHT_DEMO_EXPORTS)
endif()

#add_executable(dht-arrow core/dht_arrow.cpp ${BroadcastSource})
#if(WIN32)
#	target_link_libraries(dht-arrow dht-lib ws2_32)
#else()
#	target_link_libraries(dht-arrow dht-lib crypt pthread protobuf jsoncpp)
#endif()


#add_executable(dht-test selftest/dht/dht-test.cpp)
#if(WIN32)
#        target_link_libraries(dht-test dht-lib ws2_32)
#else()
#        target_link_libraries(dht-test dht-lib crypt pthread protobuf)
#endif()

