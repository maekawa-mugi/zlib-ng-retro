macro(configure_test_executable target)
    target_include_directories(${target} PRIVATE ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
    if(NOT WITH_GZFILEOP)
        set(GZFILE_PRIVATE_HDRS ${ZLIB_GZFILE_PRIVATE_HDRS})
        set(GZFILE_SRCS ${ZLIB_GZFILE_SRCS})
        list(TRANSFORM GZFILE_PRIVATE_HDRS PREPEND "${PROJECT_SOURCE_DIR}/")
        list(TRANSFORM GZFILE_SRCS PREPEND "${PROJECT_SOURCE_DIR}/")
        target_compile_definitions(${target} PRIVATE -DWITH_GZFILEOP)
        target_sources(${target} PRIVATE ${GZFILE_PRIVATE_HDRS} ${GZFILE_SRCS})
    endif()
endmacro()

# Portable contract tests for VIS1 lane math and SPARC HWCAP decode.
# This test runs on every host and never executes VIS instructions.
add_executable(sparc_vis1_models test_sparc_vis1_models.c)
target_include_directories(sparc_vis1_models PRIVATE ${PROJECT_SOURCE_DIR})
set_target_properties(sparc_vis1_models PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME sparc_vis1_models COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
    $<TARGET_FILE:sparc_vis1_models>)

# Compare VIS1 vector-byte matches with the scalar reference at all offsets.
if(BASEARCH_SPARC_FOUND AND WITH_OPTIM AND WITH_VIS1 AND HAVE_SPARC_VIS1_ASM)
    add_executable(vis1_compare256 test_vis1_compare256.c ${PROJECT_SOURCE_DIR}/arch/sparc/compare256_vis1.c)
    configure_test_executable(vis1_compare256)
    # Only the VIS source may use VIS instructions: the test harness must
    # remain runnable on SPARC CPUs without VIS when dispatch is enabled.
    set_source_files_properties(${PROJECT_SOURCE_DIR}/arch/sparc/compare256_vis1.c
        PROPERTIES COMPILE_FLAGS "${VIS1FLAG} ${NOLTOFLAG}")
    target_link_libraries(vis1_compare256 zlib-ng)
    add_test(NAME vis1_compare256 COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:vis1_compare256>)
    set_tests_properties(vis1_compare256 PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(vis1_adler32 test_vis1_adler32.c ${PROJECT_SOURCE_DIR}/arch/sparc/adler32_vis1.c)
    configure_test_executable(vis1_adler32)
    set_source_files_properties(${PROJECT_SOURCE_DIR}/arch/sparc/adler32_vis1.c
        PROPERTIES COMPILE_FLAGS "${VIS1FLAG} ${NOLTOFLAG}")
    target_link_libraries(vis1_adler32 zlib-ng)
    add_test(NAME vis1_adler32 COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:vis1_adler32>)
    set_tests_properties(vis1_adler32 PROPERTIES SKIP_RETURN_CODE 77)

    if(WITH_VIS1_CHUNKSET AND HAVE_SPARC_VIS1_CHUNKSET_ASM)
        add_executable(vis1_chunkmemset test_vis1_chunkmemset.c
            ${PROJECT_SOURCE_DIR}/arch/sparc/chunkmemset_sparc.c)
        configure_test_executable(vis1_chunkmemset)
        set_source_files_properties(${PROJECT_SOURCE_DIR}/arch/sparc/chunkmemset_sparc.c
            PROPERTIES COMPILE_FLAGS "${VIS1FLAG} ${NOLTOFLAG}")
        target_link_libraries(vis1_chunkmemset zlib-ng)
        add_test(NAME vis1_chunkmemset COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
            $<TARGET_FILE:vis1_chunkmemset>)
        set_tests_properties(vis1_chunkmemset PROPERTIES SKIP_RETURN_CODE 77)
    endif()

    if(WITH_VIS1_SLIDEHASH AND HAVE_SPARC_VIS1_SLIDEHASH_ASM)
        add_executable(vis1_slide_hash test_vis1_slide_hash.c
            ${PROJECT_SOURCE_DIR}/arch/sparc/slide_hash_vis1.c)
        configure_test_executable(vis1_slide_hash)
        set_source_files_properties(${PROJECT_SOURCE_DIR}/arch/sparc/slide_hash_vis1.c
            PROPERTIES COMPILE_FLAGS "${VIS1FLAG} ${NOLTOFLAG}")
        target_link_libraries(vis1_slide_hash zlib-ng)
        add_test(NAME vis1_slide_hash COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
            $<TARGET_FILE:vis1_slide_hash>)
        set_tests_properties(vis1_slide_hash PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()

if(TEST_STOCK_ZLIB)
    add_definitions(-DTEST_STOCK_ZLIB)
endif()

add_executable(example example.c)
configure_test_executable(example)
target_link_libraries(example zlib-ng)
add_test(NAME example COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:example>)

add_executable(switchlevels switchlevels.c)
configure_test_executable(switchlevels)
target_link_libraries(switchlevels zlib-ng)
set(SWITCHLEVELS_COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:switchlevels>)

add_executable(infcover infcover.c)
configure_test_executable(infcover)
target_link_libraries(infcover zlib-ng)
if(NOT DEFINED BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
    target_sources(infcover PRIVATE ${PROJECT_SOURCE_DIR}/inftrees.c ${PROJECT_SOURCE_DIR}/zutil.c)
endif()
add_test(NAME infcover COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:infcover>)

# Emscripten does not support large amounts of data via stdin/out
# https://github.com/emscripten-core/emscripten/issues/16755#issuecomment-1102732849
if(NOT BASEARCH_WASM32_FOUND)
    # Runs tests targeting CVEs
    include(cmake/test-cves.cmake)

    # Run tests with data files
    include(cmake/test-data.cmake)

    # Run tests targeting GitHub issues
    include(cmake/test-issues.cmake)

    # Run tests targeting tools
    include(cmake/test-tools.cmake)
endif()

if(WITH_FUZZERS)
    add_subdirectory(fuzz)
endif()

if(WITH_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(WITH_GTEST)
    # Google test requires at least C++11
    if(NOT DEFINED CMAKE_CXX_STANDARD)
        # Use older version of Google test to support older versions of GCC
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS_EQUAL 5.3)
            set(CMAKE_CXX_STANDARD 11)
        else()
            set(CMAKE_CXX_STANDARD 14)
        endif()
    endif()

    # Google test requires MSAN instrumented LLVM C++ libraries
    if(WITH_SANITIZER STREQUAL "memory")
        if(NOT DEFINED ENV{LLVM_BUILD_DIR})
            message(FATAL_ERROR "MSAN instrumented C++ libraries required!")
        endif()

        # Must set include and compile options before fetching googletest
        include_directories($ENV{LLVM_BUILD_DIR}/include $ENV{LLVM_BUILD_DIR}/include/c++/v1)
        add_compile_options(-stdlib=libc++ -g)
    elseif(NOT TARGET GTest::GTest)
        find_package(GTest)
    endif()

    if(NOT TARGET GTest::GTest)
        include(FetchContent)

        # Prevent overriding the parent project's compiler/linker settings for Windows
        set(gtest_force_shared_crt ON CACHE BOOL
            "Use shared (DLL) run-time lib even when Google Test is built as static lib." FORCE)

        # Allow specifying alternative Google test repository
        if(NOT DEFINED GTEST_REPOSITORY)
            set(GTEST_REPOSITORY https://github.com/google/googletest.git)
        endif()
        if(NOT DEFINED GTEST_TAG)
            # Use older version of Google test to support older versions of GCC
            if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS_EQUAL 5.3)
                set(GTEST_TAG release-1.10.0)
            else()
                set(GTEST_TAG v1.16.0)
            endif()
        endif()

        # Fetch Google test source code from official repository
        message(STATUS "Git checking out GoogleTest ${GTEST_TAG}")
        FetchContent_Declare(googletest
            GIT_REPOSITORY ${GTEST_REPOSITORY}
            GIT_TAG ${GTEST_TAG}
            ${ZNG_FetchContent_Declare_EXCLUDE_FROM_ALL})

        ZNG_FetchContent_MakeAvailable(googletest)
        add_library(GTest::GTest ALIAS gtest)
        add_library(GTest::Main ALIAS gtest_main)
    endif()

    if(TARGET GTest::GTest)
        set(TEST_SRCS
            test_chunked.cc
            test_compress.cc
            test_compress_bound.cc
            test_cve-2003-0107.cc
            test_deflate_bound.cc
            test_deflate_copy.cc
            test_deflate_dict.cc
            test_deflate_gh_361.cc
            test_deflate_gh_364.cc
            test_deflate_hash_head_0.cc
            test_deflate_header.cc
            test_deflate.cc
            test_deflate_params.cc
            test_deflate_pending.cc
            test_deflate_prime.cc
            test_deflate_quick_bi_valid.cc
            test_deflate_quick_block_open.cc
            test_deflate_quick_gh_382.cc
            test_deflate_tune.cc
            test_dict.cc
            test_dict_params.cc
            test_inflate_adler32.cc
            test_inflate_back.cc
            test_inflate_copy.cc
            test_inflate_cve.cc
            test_inflate_gh_1600.cc
            test_raw.cc
            test_small_window.cc
            )

        if(WITH_GZFILEOP)
            list(APPEND TEST_SRCS test_gzio.cc)
        endif()

        if(NOT TEST_STOCK_ZLIB)
            list(APPEND TEST_SRCS
                test_adler32.cc               # adler32_neon(), etc
                test_adler32_copy.cc          # adler32_copy implementations
                test_compare256.cc            # compare256_neon(), etc
                test_compare256_rle.cc        # compare256_rle(), etc
                test_crc32.cc                 # crc32_armv8(), etc
                test_crc32_copy.cc            # crc32_copy implementations
                test_deflate_deterministic.cc # deterministic output after deflateReset
                test_inflate_sync.cc          # expects a certain compressed block layout
                test_main.cc                  # cpu_check_features()
                test_version.cc               # expects a fixed version string
                )
        endif()

        add_executable(gtest_zlib ${TEST_SRCS})
        configure_test_executable(gtest_zlib)

        if(MSVC)
            target_compile_options(gtest_zlib PRIVATE /wd4389 /EHsc)
        endif()

        if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
            target_compile_options(gtest_zlib PRIVATE -D_XOPEN_SOURCE=700)
        endif()

        if(WITH_SANITIZER STREQUAL "memory")
            target_link_directories(gtest_zlib PRIVATE $ENV{LLVM_BUILD_DIR}/lib)
            target_link_options(gtest_zlib PRIVATE
                -stdlib=libc++
                -lc++abi
                -fsanitize=memory
                -fsanitize-memory-track-origins)
        endif()

        if(NOT ZLIB_COMPAT AND DEFINED ZLIB_LIBRARIES AND DEFINED ZLIB_INCLUDE_DIRS)
            if(NOT IS_ABSOLUTE ${ZLIB_LIBRARIES})
                get_filename_component(ZLIB_ABSOLUTE_PATH
                    "${CMAKE_CURRENT_SOURCE_DIR}/${ZLIB_LIBRARIES}"
                    ABSOLUTE)
            else()
                set(ZLIB_ABSOLUTE_PATH ${ZLIB_LIBRARIES})
            endif()

            add_library(external_zlib STATIC IMPORTED)
            set_property(TARGET external_zlib PROPERTY IMPORTED_LOCATION ${ZLIB_ABSOLUTE_PATH})
            message(STATUS "Added dual linking tests against zlib")
            message(STATUS "  Zlib include dirs: ${ZLIB_INCLUDE_DIRS}")
            message(STATUS "  Zlib libraries: ${ZLIB_ABSOLUTE_PATH}")

            target_sources(gtest_zlib PRIVATE test_compress_dual.cc)
            target_include_directories(gtest_zlib PRIVATE ${ZLIB_INCLUDE_DIRS})
            target_link_libraries(gtest_zlib external_zlib)
        endif()

        if(NOT DEFINED BUILD_SHARED_LIBS)
            # Link statically in order to test internal zlib-ng functions.
            target_link_libraries(gtest_zlib zlib-ng-static)
        else()
            target_link_libraries(gtest_zlib zlib-ng)
        endif()

        if(BUILD_SHARED_LIBS)
            target_link_libraries(gtest_zlib GTest::Main)
        endif()
        target_link_libraries(gtest_zlib GTest::GTest)

        find_package(Threads)
        if(Threads_FOUND AND NOT BASEARCH_WASM32_FOUND)
            target_sources(gtest_zlib PRIVATE test_deflate_concurrency.cc)
            if(UNIX AND NOT APPLE)
                # On Linux, use a workaround for https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52590
                target_link_libraries(gtest_zlib -Wl,--whole-archive -lpthread -Wl,--no-whole-archive)
            endif()
            target_link_libraries(gtest_zlib Threads::Threads)
        endif()

        add_test(NAME gtest_zlib
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:gtest_zlib>)
    endif()
endif()
