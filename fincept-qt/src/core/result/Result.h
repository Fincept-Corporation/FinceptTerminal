#pragma once
#include <variant>
#include <string>
#include <functional>

namespace fincept {

/// Lightweight Result<T> for error handling without exceptions.
template <typename T>
class Result {
public:
    static Result ok(T value) { return Result{std::move(value)}; }
    static Result err(std::string message) { return Result{Error{std::move(message)}}; }

    bool is_ok() const { return std::holds_alternative<T>(data_); }
    bool is_err() const { return !is_ok(); }

    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }
    const std::string& error() const { return std::get<Error>(data_).message; }

    template <typename F>
    auto map(F&& f) const -> Result<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));
        if (is_ok()) return Result<U>::ok(f(value()));
        return Result<U>::err(error());
    }

private:
    struct Error { std::string message; };
    std::variant<T, Error> data_;

    explicit Result(T val) : data_(std::move(val)) {}
    explicit Result(Error e) : data_(std::move(e)) {}
};

/// Specialization for void results.
template <>
class Result<void> {
public:
    static Result ok() { return Result{true}; }
    static Result err(std::string message) { return Result{std::move(message)}; }

    bool is_ok() const { return ok_; }
    bool is_err() const { return !ok_; }
    const std::string& error() const { return error_; }

private:
    bool ok_;
    std::string error_;

    explicit Result(bool) : ok_(true) {}
    explicit Result(std::string e) : ok_(false), error_(std::move(e)) {}
};

} // namespace fincept
