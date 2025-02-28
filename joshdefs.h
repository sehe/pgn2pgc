#pragma once
#include <cassert>
#include <memory>
template <class T> using AdoptArray = std::unique_ptr<T[]>;
