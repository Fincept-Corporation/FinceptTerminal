#pragma once
// Result<T> — lightweight error handling without exceptions
// Use instead of raw error codes or throwing exceptions

#include <string>
#include <variant>
#include <stdexcept>

namespace fincept::core {

struct Error {
    std::string message;
    int code = 0;

    Error() = default;
    explicit Error(std::string msg, int c = 0) : message(std::move(msg)), code(c) {}
};

template <typename T>
class Result {
public:
    // Success construction
    Result(T value) : data_(std::move(value)) {}

    // Error construction
    Result(Error err) : data_(std::move(err)) {}

    bool ok() const { return std::holds_alternative<T>(data_); }
    bool failed() const { return !ok(); }

    const T& value() const {
        if (failed()) throw std::runtime_error("Result::value() called on error: " + error().message);
        return std::get<T>(data_);
    }

    T& value() {
        if (failed()) throw std::runtime_error("Result::value() called on error: " + error().message);
        return std::get<T>(data_);
    }

    const Error& error() const { return std::get<Error>(data_); }

    // Value or default
    T value_or(T fallback) const {
        return ok() ? std::get<T>(data_) : std::move(fallback);
    }

    explicit operator bool() const { return ok(); }

private:
    std::variant<T, Error> data_;
};

// Void result specialization
template <>
class Result<void> {
public:
    Result() : err_(std::nullopt) {}
    Result(Error err) : err_(std::move(err)) {}

    bool ok() const { return !err_.has_value(); }
    bool failed() const { return err_.has_value(); }
    const Error& error() const { return *err_; }
    explicit operator bool() const { return ok(); }

private:
    std::optional<Error> err_;
};

} // namespace fincept::core
