include(FetchContent)

FetchContent_Declare(
        opensmt
        GIT_REPOSITORY https://github.com/usi-verification-and-security/opensmt.git
)

FetchContent_GetProperties(opensmt)
if(NOT opensmt_POPULATED)
    FetchContent_Populate(opensmt)
    add_subdirectory(${opensmt_SOURCE_DIR} ${opensmt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

find_package(GMP REQUIRED)

include_directories(${OPENSMT2_SOURCE_DIR} ${GMP_INCLUDE_DIR})

add_executable(solver_opensmt ../main.cpp SolverProcess.cpp OpenSMTSolver.cpp)

#if (NOT OPENSMT2_FOUND)
#    add_dependencies(solver_opensmt OpenSMT2)
#endif ()

set(solver_opensmt_flags "-w")

set_target_properties(solver_opensmt PROPERTIES
        COMPILE_FLAGS ${solver_opensmt_flags}
        )

    target_link_libraries(solver_opensmt client ${OPENSMT2_LIBRARY}
        ${GMP_LIBRARY} ${GMPXX_LIBRARY} z)
