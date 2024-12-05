"""Commonly used copt for benchmarking.

This macro provides a convenient way to enable benchmarking settings in C++ code.

Note: C++ style guides generally discourage the use of preprocessor macros.
This macro should be used only when necessary, such as for guarding performance sensitive code.
"""
BENCHMARK_COPT = select(
    {
        "//cc:enable_benchmarking_setting": [
            "-DPBS_ENABLE_BENCHMARKING=1",
        ],
        "//cc:disable_benchmarking_setting": [],
        "//conditions:default": [],
    },
)
