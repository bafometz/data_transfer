cmake_minimum_required(VERSION 3.5)

project(DataTransfer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Werror)
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../build/bin)
set (LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/../build/bin)

add_executable(DataTransfer

               sources/main.cpp
               sources/server/server.h sources/server/server.cpp
               sources/client/client.h sources/client/client.cpp
               sources/main_object/mainobject.h sources/main_object/mainobject.cpp
               sources/io_device/iodevice.h
               sources/socket/socket.h sources/socket/socket.cpp
               sources/data_package/datatpackage.h sources/data_package/datatpackage.cpp
               sources/logger/logger.h
               sources/thread_pool/threadpool.h
               sources/helpers/helpers.h sources/helpers/helpers.cpp
               sources/event_loop/eventloop.h sources/event_loop/eventloop.cpp
               sources/file_send_state/transmittionStatus.h
               sources/session/session.h sources/session/session.cpp
               sources/time/time.h sources/time/time.cpp
)

include(GNUInstallDirs)

install(TARGETS DataTransfer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
