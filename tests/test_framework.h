#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

// A minimal header-only test harness — enough for assertions, named cases and
// a pass/fail summary, without pulling in a dependency for a CLI project.
//
//   TEST_CASE("adds two numbers") {
//       CHECK_EQ(2 + 2, 4);
//   }
//   int main() { return testing::run(); }

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct Case {
    std::string name;
    std::function<void()> body;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

inline std::string& currentCase() {
    static std::string name;
    return name;
}

inline void fail(const char* file, int line, const std::string& message) {
    ++failureCount();
    std::cout << "    FAIL  " << message << "\n          at " << file << ":" << line << "\n";
}

// Registers a case at static-initialisation time.
struct Registrar {
    Registrar(const std::string& name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

inline int run() {
    int failedCases = 0;
    std::cout << "Running " << registry().size() << " test case(s)\n\n";

    for (const Case& testCase : registry()) {
        currentCase() = testCase.name;
        const int before = failureCount();
        try {
            testCase.body();
        } catch (const std::exception& error) {
            fail(__FILE__, __LINE__, std::string("unexpected exception: ") + error.what());
        }
        const bool passed = failureCount() == before;
        if (!passed) ++failedCases;
        std::cout << (passed ? "  ok    " : "  FAILED ") << testCase.name << "\n";
    }

    std::cout << "\n"
              << (registry().size() - failedCases) << " passed, " << failedCases << " failed, "
              << failureCount() << " assertion failure(s)\n";
    return failedCases == 0 ? 0 : 1;
}

}  // namespace testing

// __LINE__ only expands before ## with a second level of indirection.
#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                              \
    static void TEST_CONCAT(testFn_, __LINE__)();                    \
    static ::testing::Registrar TEST_CONCAT(testReg_, __LINE__)(     \
        name, TEST_CONCAT(testFn_, __LINE__));                       \
    static void TEST_CONCAT(testFn_, __LINE__)()

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) ::testing::fail(__FILE__, __LINE__, "expected: " #expr);   \
    } while (false)

#define CHECK_FALSE(expr)                                                          \
    do {                                                                           \
        if (expr) ::testing::fail(__FILE__, __LINE__, "expected NOT: " #expr);      \
    } while (false)

#define CHECK_EQ(actual, expected)                                             \
    do {                                                                       \
        const auto actualValue = (actual);                                     \
        const auto expectedValue = (expected);                                 \
        if (!(actualValue == expectedValue)) {                                 \
            std::ostringstream message;                                        \
            message << #actual " == " #expected " -- got " << actualValue       \
                    << ", want " << expectedValue;                             \
            ::testing::fail(__FILE__, __LINE__, message.str());                \
        }                                                                      \
    } while (false)

// Floating point comparison with a tolerance, for GPA maths.
#define CHECK_NEAR(actual, expected, tolerance)                                \
    do {                                                                       \
        const double actualValue = (actual);                                   \
        const double expectedValue = (expected);                               \
        if (std::fabs(actualValue - expectedValue) > (tolerance)) {            \
            std::ostringstream message;                                        \
            message << #actual " ~= " #expected " -- got " << actualValue       \
                    << ", want " << expectedValue << " (+/-" << (tolerance) << ")"; \
            ::testing::fail(__FILE__, __LINE__, message.str());                \
        }                                                                      \
    } while (false)

#endif  // TEST_FRAMEWORK_H
