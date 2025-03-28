add_executable(mutex_protected_benchmark "")
target_sources(mutex_protected_benchmark
    PRIVATE
        mutex_protected_benchmark.cc
)
target_link_libraries(mutex_protected_benchmark
    PRIVATE
        mutex_protected
        benchmark::benchmark_main
        common_compiler_settings
)
