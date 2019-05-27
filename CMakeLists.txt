PROJECT(project)
cmake_minimum_required(VERSION 2.6)
set(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall  -O2")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -O2")
set(CMAKE_CXX_FLAGS "-fPIC")  
set(CMAKE_C_FLAGS "-fPIC") 
add_definitions(-D__STDC_FORMAT_MACROS)
#add_definitions(-DNDEBUG)
add_definitions(-std=c++11  )#-fno-rtti
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/logging)
include_directories(${CMAKE_SOURCE_DIR}/congestion)
set(dqc_LIB 
src/ack_frame.cc
src/process_alarm_factory.cc   
src/random.cc
src/alarm.cc            
src/proto_con.cc               
src/received_packet_manager.cc
src/byte_codec.cc       
src/proto_error_codes.cc       
src/rtt_stats.cc
src/flag_impl.cc        
src/proto_framer.cc            
src/send_packet_manager.cc
src/ip_address.cc       
src/proto_stream.cc            
src/socket_address.cc
src/memslice.cc         
src/proto_stream_sequencer.cc 
src/send_receive.cc 
src/socket.cc
src/netutils.cc         
src/proto_time.cc              
src/unacked_packet_map.cc
src/platform_linux.c    
src/proto_types.cc
src/platform_windows.c  
src/proto_utils.cc
fun_test.cc
congestion/proto_bandwidth_sampler.cc 
congestion/proto_send_algorithm_interface.cc
congestion/proto_bbr_sender.cc  
)

add_library(dqc STATIC ${dqc_LIB})

set(EXECUTABLE_NAME "my_test")
add_executable(${EXECUTABLE_NAME} main.cpp)
target_link_libraries(${EXECUTABLE_NAME} dqc pthread)

