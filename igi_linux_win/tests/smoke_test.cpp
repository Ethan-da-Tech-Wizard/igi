// Program: igi - smoke_test.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Minimal smoke test that proves the test binary builds, links and runs.

#include <gtest/gtest.h>

// Passes as long as the test program builds and runs.
TEST(Smoke, BuildPipelineLinks) {
    SUCCEED() << "Igi build pipeline links and ctest can execute a binary.";
}
