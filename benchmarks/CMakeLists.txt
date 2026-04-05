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

project(benchmarks)

file(GLOB files CONFIGURE_DEPENDS "*.cpp")
list(REMOVE_ITEM files "${CMAKE_CURRENT_LIST_DIR}/benchmark_main.cpp")

set(bench_targets)
foreach(full_file ${files})
    get_filename_component(name ${full_file} NAME_WE)
    add_executable(${name} ${full_file} benchmark_main.cpp)
    target_link_libraries(${name} tlapp2 ${TLAPP2_GTEST})
    if(GTEST_INCLUDE_DIRS)
        target_include_directories(${name} PRIVATE ${GTEST_INCLUDE_DIRS})
    endif()
    list(APPEND bench_targets ${name})
endforeach()

add_custom_target(tlapp2_benchmarks DEPENDS ${bench_targets})
