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

if(TEST_STOCK_ZLIB)
    add_definitions(-DTEST_STOCK_ZLIB)
endif()

# This test exercises actual EE MMI instructions and must run on PS2 hardware.
# It links statically so the internal slide_hash symbols are available.
if(BASEARCH_MIPS_FOUND AND WITH_MMI)
    if(BUILD_SHARED_LIBS)
        message(FATAL_ERROR "The MMI slide_hash test requires BUILD_SHARED_LIBS=OFF")
    endif()
    add_executable(test_mmi_slide_hash test_mmi_slide_hash.c)
    configure_test_executable(test_mmi_slide_hash)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(test_mmi_slide_hash zlib-ng-static)
    else()
        target_link_libraries(test_mmi_slide_hash zlib-ng)
    endif()
    add_test(NAME mmi_slide_hash
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_mmi_slide_hash>)

    # Both serial and interleaved variants are linked into the MMI library.
    # This benchmark does not need a separate build or CTest entry.
    add_executable(bench_mmi_slide_hash bench_mmi_slide_hash.c)
    configure_test_executable(bench_mmi_slide_hash)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(bench_mmi_slide_hash zlib-ng-static)
    else()
        target_link_libraries(bench_mmi_slide_hash zlib-ng)
    endif()

    add_executable(test_mmi_compare256 test_mmi_compare256.c)
    configure_test_executable(test_mmi_compare256)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(test_mmi_compare256 zlib-ng-static)
    else()
        target_link_libraries(test_mmi_compare256 zlib-ng)
    endif()
    add_test(NAME mmi_compare256
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_mmi_compare256>)

    if(WITH_MMI_COMPARE64)
        add_executable(bench_mmi_compare256 bench_mmi_compare256.c)
        configure_test_executable(bench_mmi_compare256)
        if(NOT DEFINED BUILD_SHARED_LIBS)
            target_link_libraries(bench_mmi_compare256 zlib-ng-static)
        else()
            target_link_libraries(bench_mmi_compare256 zlib-ng)
        endif()
    endif()

    add_executable(bench_mmi_chunkset bench_mmi_chunkset.c)
    configure_test_executable(bench_mmi_chunkset)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(bench_mmi_chunkset zlib-ng-static)
    else()
        target_link_libraries(bench_mmi_chunkset zlib-ng)
    endif()

    add_executable(test_mmi_chunkset test_mmi_chunkset.c)
    configure_test_executable(test_mmi_chunkset)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(test_mmi_chunkset zlib-ng-static)
    else()
        target_link_libraries(test_mmi_chunkset zlib-ng)
    endif()
    add_test(NAME mmi_chunkset
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_mmi_chunkset>)

    add_executable(bench_mmi_roundtrip bench_mmi_roundtrip.c)
    configure_test_executable(bench_mmi_roundtrip)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(bench_mmi_roundtrip zlib-ng-static)
    else()
        target_link_libraries(bench_mmi_roundtrip zlib-ng)
    endif()

    add_executable(test_mmi_roundtrip test_mmi_roundtrip.c)
    configure_test_executable(test_mmi_roundtrip)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(test_mmi_roundtrip zlib-ng-static)
    else()
        target_link_libraries(test_mmi_roundtrip zlib-ng)
    endif()
    add_test(NAME mmi_roundtrip
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_mmi_roundtrip>)

    if(WITH_MMI_CHORBA)
        add_executable(test_mmi_chorba test_mmi_chorba.c)
        configure_test_executable(test_mmi_chorba)
        if(NOT DEFINED BUILD_SHARED_LIBS)
            target_link_libraries(test_mmi_chorba zlib-ng-static)
        else()
            target_link_libraries(test_mmi_chorba zlib-ng)
        endif()
        add_test(NAME mmi_chorba
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_mmi_chorba>)

        add_executable(bench_mmi_chorba bench_mmi_chorba.c)
        configure_test_executable(bench_mmi_chorba)
        if(NOT DEFINED BUILD_SHARED_LIBS)
            target_link_libraries(bench_mmi_chorba zlib-ng-static)
        else()
            target_link_libraries(bench_mmi_chorba zlib-ng)
        endif()

        add_executable(bench_mmi_chorba_copy bench_mmi_chorba_copy.c)
        configure_test_executable(bench_mmi_chorba_copy)
        if(NOT DEFINED BUILD_SHARED_LIBS)
            target_link_libraries(bench_mmi_chorba_copy zlib-ng-static)
        else()
            target_link_libraries(bench_mmi_chorba_copy zlib-ng)
        endif()
    endif()

    if(WITH_MMI_ADLER32)
        add_executable(test_mmi_adler32 test_mmi_adler32.c)
        configure_test_executable(test_mmi_adler32)
        if(NOT DEFINED BUILD_SHARED_LIBS)
            target_link_libraries(test_mmi_adler32 zlib-ng-static)
        else()
            target_link_libraries(test_mmi_adler32 zlib-ng)
        endif()
        add_test(NAME mmi_adler32
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:test_mmi_adler32>)

        add_executable(bench_mmi_adler32 bench_mmi_adler32.c)
        configure_test_executable(bench_mmi_adler32)
        if(NOT DEFINED BUILD_SHARED_LIBS)
            target_link_libraries(bench_mmi_adler32 zlib-ng-static)
        else()
            target_link_libraries(bench_mmi_adler32 zlib-ng)
        endif()

        add_executable(bench_mmi_adler32_copy bench_mmi_adler32_copy.c)
        configure_test_executable(bench_mmi_adler32_copy)
        if(NOT DEFINED BUILD_SHARED_LIBS)
            target_link_libraries(bench_mmi_adler32_copy zlib-ng-static)
        else()
            target_link_libraries(bench_mmi_adler32_copy zlib-ng)
        endif()
    endif()

    # Package every native MMI correctness test and benchmark into one EE
    # executable for one PS2 Linux validation trip. Existing standalone
    # executables continue to work unchanged: the suite compiles separate
    # object copies with the source's main() renamed to a unique entrypoint.
    add_executable(mmi_suite mmi_suite.c)
    configure_test_executable(mmi_suite)
    if(NOT DEFINED BUILD_SHARED_LIBS)
        target_link_libraries(mmi_suite zlib-ng-static)
    else()
        target_link_libraries(mmi_suite zlib-ng)
    endif()

    set(_mmi_suite_tests slide_hash compare256 chunkset roundtrip adler32_math)
    set(_mmi_suite_benches slide_hash chunkset roundtrip)
    if(WITH_MMI_COMPARE64)
        list(APPEND _mmi_suite_benches compare256)
    endif()
    if(WITH_MMI_ADLER32)
        list(APPEND _mmi_suite_tests adler32)
        list(APPEND _mmi_suite_benches adler32 adler32_copy)
    endif()
    if(WITH_MMI_CHORBA)
        list(APPEND _mmi_suite_tests chorba)
        list(APPEND _mmi_suite_benches chorba chorba_copy)
    endif()

    foreach(_mmi_kernel IN LISTS _mmi_suite_tests)
        set(_mmi_obj "mmi_suite_test_${_mmi_kernel}_obj")
        add_library(${_mmi_obj} OBJECT "test_mmi_${_mmi_kernel}.c")
        target_include_directories(${_mmi_obj} PRIVATE
            ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR})
        target_compile_definitions(${_mmi_obj} PRIVATE
            "main=mmi_suite_test_${_mmi_kernel}_main")
        target_sources(mmi_suite PRIVATE $<TARGET_OBJECTS:${_mmi_obj}>)
    endforeach()
    foreach(_mmi_kernel IN LISTS _mmi_suite_benches)
        set(_mmi_obj "mmi_suite_bench_${_mmi_kernel}_obj")
        add_library(${_mmi_obj} OBJECT "bench_mmi_${_mmi_kernel}.c")
        target_include_directories(${_mmi_obj} PRIVATE
            ${PROJECT_BINARY_DIR} ${PROJECT_SOURCE_DIR})
        target_compile_definitions(${_mmi_obj} PRIVATE
            "main=mmi_suite_bench_${_mmi_kernel}_main")
        target_sources(mmi_suite PRIVATE $<TARGET_OBJECTS:${_mmi_obj}>)
    endforeach()

    # The unified runner is intentionally NOT added to CTest by default:
    # normal cross-builds lack a PS2 Linux emulator, and standalone tests
    # are already registered. Execute the binary with --all on hardware.
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
