#pragma once
#include <format>
#include <string>
#include <type_traits>
#include <variant>
namespace geode {
struct ErrTag { std::string msg; };
struct OkVoid {};
template <class T> struct OkVal { T v; };
inline OkVoid Ok() { return {}; }
template <class T> OkVal<std::decay_t<T>> Ok(T&& v) { return {std::forward<T>(v)}; }
inline ErrTag Err(std::string s) { return {std::move(s)}; }
template <class... A> requires (sizeof...(A) > 0) ErrTag Err(std::format_string<A...> f, A&&... a) { return {std::format(f, std::forward<A>(a)...)}; }
template <class T = void> class Result {
    using V = std::conditional_t<std::is_void_v<T>, std::monostate, T>;
    struct E { std::string m; };
    std::variant<V, E> d;
public:
    Result() : d(E{"stub: API mod not available"}) {}
    Result(OkVoid) requires std::is_void_v<T> : d(V{}) {}
    template <class U> Result(OkVal<U> o) requires (!std::is_void_v<T>) : d(V(std::move(o.v))) {}
    Result(ErrTag e) : d(E{std::move(e.msg)}) {}
    bool isOk() const { return d.index() == 0; }
    bool isErr() const { return d.index() == 1; }
    V unwrap() const { return std::get<0>(d); }
    std::string unwrapErr() const { return std::get<1>(d).m; }
};
}
