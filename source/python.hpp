#pragma once

#pragma push_macro("slots")
#undef slots

#include "pybind11/embed.h"
#include "pybind11/numpy.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace pybind11
{
    class interpreter
    {
    public:
        static inline std::wstring python_home;
        static inline std::vector<std::wstring> module_search_paths;

        static void initialize();

        interpreter() noexcept = default;

        interpreter( const interpreter& ) = delete;
        interpreter( interpreter&& ) = delete;

        interpreter& operator=( const interpreter& ) = delete;
        interpreter& operator=( interpreter&& ) = delete;

        ~interpreter();

    private:
        static inline std::unique_ptr<py::scoped_interpreter> _interpreter;
    };
}

#pragma pop_macro("slots")