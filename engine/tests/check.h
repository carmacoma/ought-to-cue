// Minimal test harness: no third-party test library (plan: "no third-party C++ libraries in this stage").
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace check
{
struct Case
{
	const char* name;
	std::function<void()> fn;
};
inline std::vector<Case>& cases()
{
	static std::vector<Case> c;
	return c;
}
inline int& failures()
{
	static int f = 0;
	return f;
}
struct Registrar
{
	Registrar(const char* name, std::function<void()> fn) { cases().push_back({name, std::move(fn)}); }
};
inline int runAllTests()
{
	int failedCases = 0;
	for (auto& c : cases())
	{
		const int before = failures();
		c.fn();
		const bool ok = failures() == before;
		std::printf("%s %s\n", ok ? "PASS" : "FAIL", c.name);
		if (!ok)
			++failedCases;
	}
	std::printf("%zu tests, %d failed\n", cases().size(), failedCases);
	return failedCases == 0 ? 0 : 1;
}
}  // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
#define TEST(name)                                                              \
	static void name();                                                         \
	static check::Registrar CHECK_CAT(registrar_, name)(#name, name);          \
	static void name()
#define CHECK(cond)                                                                       \
	do                                                                                    \
	{                                                                                     \
		if (!(cond))                                                                      \
		{                                                                                 \
			std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond);       \
			++check::failures();                                                          \
		}                                                                                 \
	} while (0)
#define CHECK_NEAR(a, b, tol)                                                                                   \
	do                                                                                                          \
	{                                                                                                           \
		const double check_a_ = (a), check_b_ = (b);                                                            \
		if (!(std::fabs(check_a_ - check_b_) <= (tol)))                                                         \
		{                                                                                                       \
			std::printf("  %s:%d: CHECK_NEAR(%s, %s) failed: %.9g vs %.9g\n", __FILE__, __LINE__, #a, #b, check_a_, \
			            check_b_);                                                                              \
			++check::failures();                                                                                \
		}                                                                                                       \
	} while (0)
#define TEST_MAIN() \
	int main() { return check::runAllTests(); }
