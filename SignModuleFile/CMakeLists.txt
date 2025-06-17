# 设置CMake的最小版本
cmake_minimum_required(VERSION 3.15)

# 项目名称
#   项目可以包含多个目标(target)
#   target: 可执行程序, 静态库, 动态库
project(CloudDisk)

# 指定C++的版本: -std=c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#设置可执行程序输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

#SignModule----------------------------------------------------------------
add_executable(SignModule
    server.pb_skeleton.cc
    ../CryptoUtil.cpp
    Sign.pb.cc
)
target_compile_options(SignModule PRIVATE
    -g
)
target_link_libraries(SignModule PRIVATE 
    crypto
    ssl
    jwt
    wfrest
    srpc
    lz4
    snappy
    protobuf
    workflow
    ppconsul
)


# add_executable(test
#     client.pb_skeleton.cc
#     Sign.pb.cc
# )
# target_compile_options(test PRIVATE
#     -g
# )
# target_link_libraries(test PRIVATE 
#     srpc
#     lz4
#     snappy
#     protobuf
#     workflow
# )

