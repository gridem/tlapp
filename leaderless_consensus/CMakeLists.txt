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

project(leaderless_consensus)

add_executable(leaderless-consensus
    sore.cpp
    calm.cpp
    flat.cpp
    most.cpp
    rush.cpp
    ../samples/sample_main.cpp)
target_link_libraries(leaderless-consensus tlapp2 ${TLAPP2_GTEST})
if(GTEST_INCLUDE_DIRS)
    target_include_directories(leaderless-consensus PRIVATE ${GTEST_INCLUDE_DIRS})
endif()
if(TLAPP2_FORMAT_BEFORE_BUILD AND TARGET format)
    add_dependencies(leaderless-consensus format)
endif()

# Keep the runnable binary with the other examples.
set_target_properties(leaderless-consensus PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/samples")
