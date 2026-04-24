#ifndef OVERLOADED_H
#define OVERLOADED_H

// Helper for std::visit with multiple lambdas
// This is a standard C++17 pattern
template <typename... Ts>
struct overloaded : Ts...
{
  using Ts::operator()...;
};

// Deduction guide — needed in C++17, not needed in C++20
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#endif