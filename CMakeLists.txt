cmake_minimum_required(VERSION 3.15)

# CMake 3.15 se pehle kuch features nahi the jo hum use karenge
project(TCPServer)

# Naam set kiya — PROJECT_NAME = "TCPServer"
set(CMAKE_CXX_STANDARD 17)

# Har target pe automatically -std=c++17 lagega

# ---- Generator ----
add_custom_target(generate
    COMMAND python3 ${CMAKE_SOURCE_DIR}/generator.py

    # CMAKE_SOURCE_DIR = project root jahan CMakeLists.txt hai
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}

    # generator.py yahan se chalega — relative paths sahi rahenge
    COMMENT "Generating message classes..."

    # make chalane pe yeh message dikhega
)

# ---- Generated files collect karo ----
file(GLOB MESSAGE_SOURCES "${CMAKE_SOURCE_DIR}/Messages/*.cxx")

# ---- Server ----
add_executable(server
    main.cxx
    Message.cxx
    ProtocolHandler.cxx
    ${MESSAGE_SOURCES} # sab generated files
)
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR})

# tells the compiler where all the header files are located
target_link_libraries(server PRIVATE pthread)

add_dependencies(server generate)

# Before we make server we need to generate messages, so adding a dependecy!
# ----EpollServer-------
add_executable(epollServer
    server_kqueue.cxx
    Message.cxx
    ProtocolHandler.cxx
    ThreadPool.cxx
    ${MESSAGE_SOURCES}
)
target_include_directories(epollServer PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(epollServer PRIVATE pthread)
add_dependencies(epollServer generate)

# ---- Client ----
add_executable(client
    client.cxx
    Message.cxx
    ${MESSAGE_SOURCES}
)
target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(client PRIVATE pthread)
add_dependencies(client generate)