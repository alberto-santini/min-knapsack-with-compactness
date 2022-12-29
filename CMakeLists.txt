cmake_minimum_required(VERSION 3.16)
project(kplink)

set(CMAKE_CXX_STANDARD 17)
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(kplink
    src/BranchAndCut.h
    src/BranchAndCut.cpp
    src/BranchAndCutSeparation.h
    src/BranchAndCutSeparation.cpp
    src/CompactModel.h
    src/CompactModel.cpp
    src/GreedyHeuristic.h
    src/GreedyHeuristic.cpp
    src/InitialSolution.h
    src/InitialSolution.cpp
    src/Labelling.cpp
    src/Labelling.h
    src/Problem.cpp
    src/Problem.h
    src/UnitProfitDP.cpp
    src/UnitProfitDP.h
    src/main.cpp)

set(WARNING_OPTIONS     -Wall -Wextra -Werror)
set(GCC_WARNING_OPTIONS -Wduplicated-cond -Wlogical-op -Wuseless-cast -Wrestrict)
set(DEBUG_OPTIONS       -O0 -g)
set(GDB_DEBUG_OPTIONS   -ggdb)
set(LINKER_OPTIONS      -flto)

option(BC_DEBUG "Print debug info for the Branch-and-Cut algorithm" OFF)

find_package(Gurobi REQUIRED)

target_include_directories( kplink SYSTEM PRIVATE "src/nlohmann")
target_include_directories( kplink SYSTEM PRIVATE "src/cxxopts")
target_include_directories( kplink SYSTEM PRIVATE "src/date")
target_include_directories( kplink SYSTEM PRIVATE "src/rapidcsv")
target_include_directories( kplink SYSTEM PRIVATE ${Gurobi_INCLUDE_DIRS})

if(BC_DEBUG)
    target_compile_definitions(kplink PRIVATE BC_DEBUG)
endif()

target_compile_options(     kplink PRIVATE ${WARNING_OPTIONS})
target_compile_options(     kplink PRIVATE $<$<CXX_COMPILER_ID:GNU>:${GCC_WARNING_OPTIONS}>)
target_compile_options(     kplink PRIVATE $<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>)
target_compile_options(     kplink PRIVATE $<$<AND:$<CONFIG:DEBUG>,$<CXX_COMPILER_ID:GNU>>:${GDB_DEBUG_OPTIONS}>)
target_link_libraries(      kplink PRIVATE ${Gurobi_LIBRARIES})
target_link_libraries(      kplink PRIVATE ${LINKER_OPTIONS})
