// Logger.hpp: A header-only self-contained logger.

#ifndef IPASIM_LOGGER_HPP
#define IPASIM_LOGGER_HPP

#include <functional>
#include <iostream>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>

#if !defined(IPASIM_NO_WINDOWS_ERRORS)
// From <winnt.h>
// TODO: How are these undefined?
#define LANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01 // user default

#include <winrt/base.h>
#else
__declspec(dllimport) __stdcall void OutputDebugStringA(const char *);
__declspec(dllimport) __stdcall void OutputDebugStringW(const wchar_t *);
#endif

#define IPASIM_NORETURN [[noreturn]]

namespace ipasim {

class FatalError : public std::runtime_error {
public:
  FatalError(const char *Message) : runtime_error(Message) {}
};

struct StreamToken {};
struct EndToken : StreamToken {};
struct WinErrorToken : StreamToken {};
struct AppendWinErrorToken : StreamToken {};
struct FatalEndToken : StreamToken {
  FatalEndToken(const char *Message) : Message(Message) {}

  const char *Message;
};

// SFINAE magic
struct has_to_string {
  struct a {};
  struct b : a {};

  template <typename> static constexpr bool getValue(a) { return false; }
  template <typename T>
  static constexpr std::enable_if_t<
      std::is_same_v<decltype(std::to_string(std::declval<T>())), std::string>,
      bool>
  getValue(b) {
    return true;
  }
};
template <typename T>
constexpr bool has_to_string_v = has_to_string::getValue<T>(has_to_string::b());

struct has_ostream_op {
  struct a {};
  struct b : a {};

  template <typename> static constexpr bool getValue(a) { return false; }
  template <typename T>
  static constexpr std::enable_if_t<
      std::is_same_v<decltype(std::declval<std::ostream &>()
                              << std::declval<T>()),
                     std::ostream &>,
      bool>
  getValue(b) {
    return true;
  }
};
template <typename T>
constexpr bool
    has_ostream_op_v = has_ostream_op::getValue<T>(has_ostream_op::b());

// SFINAE magic
template <typename FromTy, typename... ToTys> struct is_invocable_any;
template <typename FromTy, typename ToTy, typename... ToTys>
struct is_invocable_any<FromTy, ToTy, ToTys...>
    : std::bool_constant<std::is_invocable_v<void(ToTy), FromTy> ||
                         is_invocable_any<FromTy, ToTys...>::value> {};
template <typename FromTy>
struct is_invocable_any<FromTy> : std::bool_constant<false> {};
template <typename FromTy, typename... ToTys>
constexpr bool is_invocable_any_v = is_invocable_any<FromTy, ToTys...>::value;

template <typename DerivedTy> class Stream {
public:
  using Handler = std::function<void(DerivedTy &)>;

  // Note that all `operator<<`s must be here and none inside `DerivedTy`,
  // because otherwise only those in `DerivedTy` would be searched.
  DerivedTy &operator<<(const char *S) {
    d().write(S);
    return d();
  }
  DerivedTy &operator<<(const wchar_t *S) {
    d().write(S);
    return d();
  }
  DerivedTy &operator<<(const std::string &S) { return d() << S.c_str(); }
  DerivedTy &operator<<(const std::wstring &S) { return d() << S.c_str(); }
  DerivedTy &operator<<(EndToken) { return d() << ".\n"; }
  DerivedTy &operator<<(WinErrorToken) {
#if !defined(IPASIM_NO_WINDOWS_ERRORS)
    using namespace winrt;

    // From `winrt::throw_last_error`
    hresult_error Err(HRESULT_FROM_WIN32(GetLastError()));
    return d() << Err.message().c_str();
#else
    return d() << "IPASIM_NO_WINDOWS_ERRORS";
#endif
  }
  DerivedTy &operator<<(AppendWinErrorToken) {
    return d() << EndToken() << WinErrorToken() << "\n";
  }
  IPASIM_NORETURN DerivedTy &operator<<(FatalEndToken T) {
    d() << EndToken();
    throw FatalError(T.Message);
  }
  DerivedTy &operator<<(const Handler &Handler) {
    Handler(d());
    return d();
  }

  // The following `operator<<`s can take any `T`, so they are enabled only if
  // other overloads of `operator<<` cannot be used, i.e., `T` cannot be
  // converted to any type they take as their argument.
  template <typename T>
  static constexpr bool others_failed_v =
      !is_invocable_any_v<T, const char *, const wchar_t *, const std::string &,
                          const std::wstring &, StreamToken, const Handler &>;

  // This `operator<<` is enabled only if `to_string(T)` exists.
  template <typename T>
  std::enable_if_t<others_failed_v<T> && has_to_string_v<T>, DerivedTy &>
  operator<<(const T &Any) {
    return d() << std::to_string(Any).c_str();
  }
  // This `operator<<` is enabled only if `to_string(T)` doesn't exist. We use
  // this as the last resort since it uses our `Buf` which writes strings
  // character after character (i.e., slowly).
  // TODO: What about `wchar_t`?
  template <typename T>
  std::enable_if_t<others_failed_v<T> && !has_to_string_v<T> &&
                       has_ostream_op_v<T>,
                   DerivedTy &>
  operator<<(const T &Any) {
    return output<T, char>(Any);
  }

protected:
  template <typename CharTy> class Buf : public std::basic_streambuf<CharTy> {
  public:
    Buf(Stream &S) : S(S) {}

  protected:
    using int_type = typename std::basic_streambuf<CharTy>::int_type;

    int_type overflow(int_type C) override {
      CharTy C0[] = {static_cast<CharTy>(C), 0};
      S << C0;
      return 0;
    }

  private:
    Stream &S;
  };

private:
  DerivedTy &d() { return *static_cast<DerivedTy *>(this); }
  template <typename T, typename CharTy> DerivedTy &output(const T &Any) {
    Buf<CharTy> Buf(*this);
    std::ostream OS(&Buf);
    OS << Any;
    return d();
  }

  static constexpr bool IsIpasimStream = true;
  friend struct is_stream;
};

// SFINAE magic
struct is_stream {
  struct a {};
  struct b : a {};

  template <typename> static constexpr bool getValue(a) { return false; }
  template <typename StreamTy, bool Result = StreamTy::IsIpasimStream>
  static constexpr bool getValue(b) {
    return Result;
  }
};
template <typename StreamTy>
constexpr bool is_stream_v = is_stream::getValue<StreamTy>(is_stream::b());

class DebugStream : public Stream<DebugStream> {
public:
  void write(const char *S) { OutputDebugStringA(S); }
  void write(const wchar_t *S) { OutputDebugStringW(S); }
};

class StdStream : public Stream<StdStream> {
public:
  StdStream(std::ostream &Str, std::wostream &WStr) : Str(Str), WStr(WStr) {}

  void write(const char *S) { Str << S; }
  void write(const wchar_t *S) { WStr << S; }

  static StdStream out() { return StdStream(std::cout, std::wcout); }
  static StdStream err() { return StdStream(std::cerr, std::wcerr); }

private:
  std::ostream &Str;
  std::wostream &WStr;
};

template <typename... StreamTys>
class AggregateStream : public Stream<AggregateStream<StreamTys...>> {
public:
  static_assert(sizeof...(StreamTys) > 0, "At least one stream is required.");

  template <typename... ArgTys>
  AggregateStream(ArgTys &&... Streams)
      : Streams(std::forward<ArgTys>(Streams)...) {}

  void write(const char *S) { write<0, char>(S); }
  void write(const wchar_t *S) { write<0, wchar_t>(S); }

private:
  std::tuple<StreamTys...> Streams;

  template <size_t I, typename C> void write(const C *S) {
    std::get<I>(Streams).write(S);
    if constexpr (I + 1 < sizeof...(StreamTys))
      write<I + 1, C>(S);
  }
};

template <typename StreamTy> class Logger {
public:
  Logger() = default;
  Logger(StreamTy &&O, StreamTy &&E) : O(std::move(O)), E(std::move(E)) {}

  void error(const std::string &Message) { error() << Message << end(); }
  void info(const std::string &Message) { info() << Message << end(); }
  void warning(const std::string &Message) { warning() << Message << end(); }
  void winError(const std::string &Message) {
    error(Message);
    errs() << winError() << "\n";
  }
  IPASIM_NORETURN void fatalError(const std::string &Message) {
    error(Message);
    throw FatalError(Message.c_str());
  }
  StreamTy &errs() { return E; }
  StreamTy &infs() { return O; }
  StreamTy &warns() { return E; }
  StreamTy &error() { return errs() << "Error: "; }
  StreamTy &info() { return infs() << "Info: "; }
  StreamTy &warning() { return warns() << "Warning: "; }
  EndToken end() { return EndToken(); }
  WinErrorToken winError() { return WinErrorToken(); }
  AppendWinErrorToken appendWinError() { return AppendWinErrorToken(); }
  FatalEndToken fatalEnd(const char *Message) { return FatalEndToken(Message); }
  FatalEndToken fatalEnd() { return FatalEndToken("Fatal error occurred."); }

private:
  StreamTy O, E;
};

using DebugLogger = Logger<DebugStream>;
using StdLogger = Logger<StdStream>;

} // namespace ipasim

// !defined(IPASIM_LOGGER_HPP)
#endif
