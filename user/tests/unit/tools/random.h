#ifndef TEST_TOOLS_RANDOM_H
#define TEST_TOOLS_RANDOM_H

#include <random>
#include <string>

namespace test {

int randomInt(int min, int max); // [min, max]
double randomDouble(double min, double max); // [min, max)

std::string randomString(size_t size); // Generates printable string
std::string randomString(size_t minSize, size_t maxSize);
std::string randomBytes(size_t size);
std::string randomBytes(size_t minSize, size_t maxSize);

std::default_random_engine& randomGenerator(); // Returns thread-local instance

} // namespace test

inline std::string test::randomString(size_t minSize, size_t maxSize) {
    return randomString(randomInt(minSize, maxSize));
}

inline std::string test::randomBytes(size_t minSize, size_t maxSize) {
    return randomBytes(randomInt(minSize, maxSize));
}

#endif // TEST_TOOLS_RANDOM_H
