# Copyright 2019-2026 Grigory Demchenko (aka gridem)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.12)
project(tlapp2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glog REQUIRED)
find_package(GTest REQUIRED)

include(GoogleTest)

if(TARGET GTest::gtest)
    set(TLAPP2_GTEST GTest::gtest)
elseif(TARGET GTest::GTest)
    set(TLAPP2_GTEST GTest::GTest)
else()
    set(TLAPP2_GTEST ${GTEST_LIBRARIES})
endif()

# Add address sanitizer for debug build
set(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} \
        -fno-omit-frame-pointer \
        -fno-optimize-sibling-calls \
        -fsanitize=address \
        -fsanitize-address-use-after-scope")

file(GLOB_RECURSE PRJ_SRC
    CONFIGURE_DEPENDS
    "src/*"
)

# file(GLOB_RECURSE PRJ_APP
#     "app/*"
# )

file(GLOB_RECURSE PRJ_TESTS
    CONFIGURE_DEPENDS
    "tests/*"
)

enable_testing()

add_library(${PROJECT_NAME} ${PRJ_SRC})
target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} glog::glog)

# add_executable(${PROJECT_NAME}_app ${PRJ_APP})

add_executable(${PROJECT_NAME}_tests ${PRJ_TESTS})
target_link_libraries(${PROJECT_NAME}_tests
    ${PROJECT_NAME} ${TLAPP2_GTEST})
if(GTEST_INCLUDE_DIRS)
    target_include_directories(${PROJECT_NAME}_tests PRIVATE ${GTEST_INCLUDE_DIRS})
endif()

add_subdirectory(samples)
add_subdirectory(leaderless_consensus)
add_subdirectory(benchmarks)

#gtest_add_tests(TARGET ${PROJECT_NAME}_tests)
