cmake_minimum_required(VERSION 3.20)
project(fourthlab)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(FOURTHLAB_HEADERS
        classes/headers/ArraySequence.h
        classes/headers/Cardinal.h
        classes/headers/DynamicArray.h
        classes/headers/ForecastCorrection.h
        classes/headers/IEnumerator.h
        classes/headers/LazySequence.h
        classes/headers/LinkedList.h
        classes/headers/ListSequence.h
        classes/headers/MutableArraySequence.h
        classes/headers/MutableListSequence.h
        classes/headers/Option.h
        classes/headers/Sequence.h
        classes/headers/Streams.h
        classes/headers/UI.h
)

add_executable(fourthlab
        main.cpp
        classes/cpp/UI.cpp
        ${FOURTHLAB_HEADERS}
)

target_include_directories(fourthlab PRIVATE classes/headers)

enable_testing()

if(NOT TARGET GTest::gtest_main)
        add_subdirectory(C:/Users/Vasya/CLionProjects/secondlab/googletest
                ${CMAKE_BINARY_DIR}/googletest)
endif()

add_executable(lab4_tests
        tests/lab4_tests.cpp
        ${FOURTHLAB_HEADERS}
)

target_include_directories(lab4_tests PRIVATE classes/headers)
target_link_libraries(lab4_tests PRIVATE GTest::gtest_main)

add_test(NAME lab4_tests COMMAND lab4_tests)
