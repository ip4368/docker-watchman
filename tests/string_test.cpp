/* Copyright 2016-present Facebook, Inc.
 * Licensed under the Apache License, Version 2.0. */

#include "watchman.h"
#include "watchman_string.h"
#include <string>
#include "thirdparty/tap.h"

void test_integrals() {
  ok(w_string::build(int8_t(1)) == w_string("1"), "made 1");
  ok(w_string::build(int16_t(1)) == w_string("1"), "made 1");
  ok(w_string::build(int32_t(1)) == w_string("1"), "made 1");
  ok(w_string::build(int64_t(1)) == w_string("1"), "made 1");

  ok(w_string::build(int8_t(-1)) == w_string("-1"), "made -1");
  ok(w_string::build(int16_t(-1)) == w_string("-1"), "made -1");
  ok(w_string::build(int32_t(-1)) == w_string("-1"), "made -1");
  ok(w_string::build(int64_t(-1)) == w_string("-1"), "made -1");

  ok(w_string::build(uint8_t(1)) == w_string("1"), "made 1");
  ok(w_string::build(uint16_t(1)) == w_string("1"), "made 1");
  ok(w_string::build(uint32_t(1)) == w_string("1"), "made 1");
  ok(w_string::build(uint64_t(1)) == w_string("1"), "made 1");

  ok(w_string::build(uint8_t(255)) == w_string("255"), "made 255");
  ok(w_string::build(uint16_t(255)) == w_string("255"), "made 255");
  ok(w_string::build(uint32_t(255)) == w_string("255"), "made 255");
  ok(w_string::build(uint64_t(255)) == w_string("255"), "made 255");

  ok(w_string::build(int8_t(-127)) == w_string("-127"), "made -127");

  ok(w_string::build(bool(true)) == w_string("1"), "true -> 1");
  ok(w_string::build(bool(false)) == w_string("0"), "false -> 0");
}

void test_strings() {
  {
    auto hello = w_string::build("hello");
    ok(hello == w_string("hello"), "hello");
    ok(hello.size() == 5, "there are 5 chars in hello");
    ok(!strcmp("hello", hello.c_str()),
       "looks nul terminated `%s` %" PRIu32,
       hello.c_str(),
       strlen_uint32(hello.c_str()));
  }

  {
    w_string_piece piece("hello");
    ok(piece.size() == 5, "piece has 5 char size");
    auto hello = w_string::build(piece);
    ok(hello.size() == 5, "hello has 5 char size");
    ok(!strcmp("hello", hello.c_str()), "looks nul terminated");
  }

  {
    char foo[] = "foo";
    auto str = w_string::build(foo);
    ok(str.size() == 3, "foo has 3 char size");
    ok(!str.empty(), "foo is not empty");
    ok(!strcmp("foo", foo), "foo matches");
  }

  {
    w_string defaultStr;
    ok(defaultStr.empty(), "default constructed string should be empty");

    w_string nullStr(nullptr);
    ok(nullStr.empty(), "nullptr string should be empty");

    ok(w_string_piece().empty(),
       "default constructed string piece shouldbe empty");

    ok(w_string_piece(nullptr).empty(),
       "nullptr string piece shouldbe empty");

    ok(w_string::build("").empty(), "empty string is empty");
  }
}

void test_pointers() {
  bool foo = true;
  char lowerBuf[20];

  auto str = w_string::build(&foo);
  snprintf(
      lowerBuf, sizeof(lowerBuf), "0x%" PRIx64, (uint64_t)(uintptr_t)(&foo));
  ok(str.size() == strlen_uint32(lowerBuf),
     "reasonable seeming bool pointer len, got %" PRIu32
     " vs expected %" PRIu32,
     str.size(),
     strlen_uint32(lowerBuf));
  ok(str.size() == strlen_uint32(str.c_str()),
     "string is really nul terminated, size %" PRIu32
     " strlen of c_str %" PRIu32,
     str.size(),
     strlen_uint32(str.c_str()));
  ok(!strcmp(lowerBuf, str.c_str()),
     "bool pointer rendered right hex value sprintf->%s, str->%s",
     lowerBuf,
     str.c_str());

  str = w_string::build(nullptr);
  ok(str.size() > 0, "nullptr has reasonable size: %" PRIsize_t, str.size());
  ok(str == w_string("0x0"), "nullptr looks right %s", str.c_str());

  void* zero = 0;
  ok(w_string::build(zero) == "0x0", "zero pointer looks right");
}

void test_double() {
  auto str = w_string::build(5.5);
  char buf[16];
  snprintf(buf, sizeof(buf), "%f", 5.5);
  ok(str.size() == 8, "size is %" PRIsize_t, str.size());
  ok(!strcmp(str.c_str(), buf), "str.c_str=%s, buf=%s", str.c_str(), buf);
  ok(str == w_string("5.500000"), "double looks good '%s'", str.c_str());
}

void test_concat() {
  auto str = w_string::build("one", 2, "three", 1.2, false, w_string(nullptr));
  ok(str == w_string("one2three1.2000000"), "concatenated to %s", str.c_str());
}

void test_lowercase_suffix() {
  ok(!w_string("").asLowerCaseSuffix(), "empty string suffix");
  ok(w_string(".").asLowerCaseSuffix() == w_string(nullptr), "only one dot suffix");
  ok(w_string("endwithdot.").asLowerCaseSuffix() == w_string(nullptr), "end with dot");
  ok(!w_string("nosuffix").asLowerCaseSuffix(), "no suffix");
  ok(w_string(".beginwithdot").asLowerCaseSuffix() == w_string("beginwithdot"),
     "begin with dot");
  ok(w_string("MainActivity.java").asLowerCaseSuffix() == w_string("java"), "java suffix");
  ok(w_string("README.TXT").asLowerCaseSuffix() == w_string("txt"), "TXT suffix");
  ok(w_string("README.camelCaseSuffix").asLowerCaseSuffix() == w_string("camelcasesuffix"), "camelCaseSuffix suffix");
  ok(w_string("foo/bar").asLowerCaseSuffix() == w_string(nullptr), "slash test no ext");
  ok(w_string("foo.wat/bar").asLowerCaseSuffix() == w_string(nullptr), "slash test no file ext");
  ok(w_string("foo.wat/bar.xml").asLowerCaseSuffix() == w_string("xml"), "slash test ext");
  ok(w_string("foo\\bar").asLowerCaseSuffix() == w_string(nullptr), "back slash test no ext");
  ok(w_string("foo\\bar.lU").asLowerCaseSuffix() == w_string("lu"), "back slash test file ext");

#ifdef _WIN32
  ok(w_string("foo.wat\\bar").asLowerCaseSuffix() == w_string(nullptr), "win back slash test no file ext");
#else
  ok(w_string("foo.wat\\bar").asLowerCaseSuffix() == w_string("wat\\bar"), "nix back slash test file ext");
#endif

  // 255 is the longest suffix among some systems
  std::string longName(255, 'a');
  auto str = w_string::build(".", longName.c_str());
  ok(str.asLowerCaseSuffix().size() == 255, "length 255");
}

void test_string_piece_suffix() {
  ok(w_string_piece().suffix() == nullptr, "null string piece");
  ok(w_string_piece("").suffix() == nullptr, "piece empty string suffix");
  ok(w_string_piece(".").suffix() == nullptr, "piece only one dot suffix");
  ok(w_string_piece("endwithdot.").suffix() == nullptr, "piece end with dot");
  ok(w_string_piece("nosuffix").suffix() == nullptr, "piece no suffix");
  ok(w_string_piece(".beginwithdot").suffix() == w_string_piece("beginwithdot"), "piece begin with dot");
  ok(w_string_piece("MainActivity.java").suffix() == w_string_piece("java"), "piece java suffix");
  ok(w_string_piece("README.TXT").suffix() == w_string_piece("TXT"), "piece TXT suffix");
  ok(w_string_piece("README.camelCaseSuffix").suffix() == w_string_piece("camelCaseSuffix"), "piece camelCaseSuffix suffix");
  ok(w_string_piece("foo/bar").suffix() == w_string_piece(nullptr), "piece slash test no ext");
  ok(w_string_piece("foo.wat/bar").suffix() == w_string_piece(nullptr), "piece slash test no file ext");
  ok(w_string_piece("foo.wat/bar.xml").suffix() == w_string_piece("xml"), "piece slash test ext");
  ok(w_string_piece("foo\\bar").suffix() == w_string_piece(nullptr), "piece back slash test no ext");
  ok(w_string_piece("foo\\bar.lU").suffix() == w_string_piece("lU"), "piece back slash test file ext");

#ifdef _WIN32
  ok(w_string_piece("foo.wat\\bar").suffix() == w_string_piece(nullptr), "win piece back slash test no file ext");
#else
  ok(w_string_piece("foo.wat\\bar").suffix() == w_string_piece("wat\\bar"), "nix piece back slash test no file ext");
#endif

  // 255 is the longest suffix among some systems
  std::string longName(255, 'a');
  auto str = w_string::build(".", longName.c_str());
  auto sp = w_string_piece(str.data(), str.size());
  ok(sp.asLowerCaseSuffix().size() == 255, "piece length 255");
}

void test_string_piece_lowercase_suffix() {
  ok(w_string_piece().asLowerCaseSuffix() == w_string(nullptr), "piece lc null string piece");
  ok(w_string_piece("").asLowerCaseSuffix() == w_string(nullptr), "piece lc empty string suffix");
  ok(w_string_piece(".").asLowerCaseSuffix() == w_string(nullptr), "piece lc only one dot suffix");
  ok(w_string_piece("endwithdot.").asLowerCaseSuffix() == w_string(nullptr), "piece lc end with dot");
  ok(!w_string_piece("nosuffix").asLowerCaseSuffix(), "piece lc no suffix");
  ok(w_string_piece(".beginwithdot").asLowerCaseSuffix() == w_string("beginwithdot"), "piece lc begin with dot");
  ok(w_string_piece("MainActivity.java").asLowerCaseSuffix() == w_string("java"), "piece lc java suffix");
  ok(w_string_piece("README.TXT").asLowerCaseSuffix() == w_string("txt"), "piece lc TXT suffix");
  ok(w_string_piece("README.camelCaseSuffix").asLowerCaseSuffix() == w_string("camelcasesuffix"), "piece lc camelCaseSuffix suffix");
  ok(w_string_piece("foo/bar").asLowerCaseSuffix() == w_string(nullptr), "piece lc slash test no ext");
  ok(w_string_piece("foo.wat/bar").asLowerCaseSuffix() == w_string(nullptr), "piece lc slash test no file ext");
  ok(w_string_piece("foo.wat/bar.xml").asLowerCaseSuffix() == w_string("xml"), "piece lc slash test ext");
  ok(w_string_piece("foo\\bar").asLowerCaseSuffix() == w_string(nullptr), "piece lc back slash test no ext");
  ok(w_string_piece("foo\\bar.lU").asLowerCaseSuffix() == w_string("lu"), "piece lc back slash test file ext");

#ifdef _WIN32
  ok(w_string_piece("foo.wat\\bar").asLowerCaseSuffix() == w_string(nullptr), "win piece lc back slash test no file ext");
#else
  ok(w_string_piece("foo.wat\\bar").asLowerCaseSuffix() == w_string("wat\\bar"), "nix piece lc back slash test no file ext");
#endif

  // 255 is the longest suffix among some systems
  std::string longName(255, 'a');
  auto str = w_string::build(".", longName.c_str());
  auto sp = w_string_piece(str.c_str(), str.size());
  ok(sp.asLowerCaseSuffix().size() == 255, "piece lc length 255");
}

void test_to() {
  auto str = watchman::to<std::string>("foo", 123);
  ok(str == "foo123", "concatenated to foo123: %s", str.c_str());
  ok(str.size() == 6, "got size %d", int(str.size()));
}

void test_path_cat() {
  auto str = w_string::pathCat({"foo", ""});
  ok(str == "foo", "concat yields %s", str.c_str());

  str = w_string::pathCat({"", "foo"});
  ok(str == "foo", "concat yields %s", str.c_str());

  str = w_string::pathCat({"foo", "bar"});
  ok(str == "foo/bar", "concat yields %s", str.c_str());

  str = w_string::pathCat({"foo", "bar", ""});
  ok(str == "foo/bar", "concat yields %s", str.c_str());

  str = w_string::pathCat({"foo", "", "bar"});
  ok(str == "foo/bar", "concat yields %s", str.c_str());
}

void test_basename_dirname() {
  auto str = w_string_piece("foo/bar").baseName().asWString();
  ok(str == "bar", "basename of foo/bar is bar: %s", str.c_str());

  str = w_string_piece("foo/bar").dirName().asWString();
  ok(str == "foo", "dirname of foo/bar is foo: %s", str.c_str());

  str = w_string_piece("").baseName().asWString();
  ok(str == "", "basename of empty string is empty: %s", str.c_str());

  str = w_string_piece("").dirName().asWString();
  ok(str == "", "dirname of empty string is empty: %s", str.c_str());

  str = w_string_piece("foo").dirName().asWString();
  ok(str == "", "dirname of foo is nothing: %s", str.c_str());

  str = w_string("f/b/z");
  auto piece = str.piece().dirName();
  auto str2 = piece.baseName().asWString();
  ok(str2 == "b", "basename of dirname of f/b/z is b: %s", str.c_str());

  str = w_string_piece("foo/bar/baz").dirName().dirName().asWString();
  ok(str == "foo", "dirname of dirname of foo/bar/baz is foo: %s", str.c_str());

  str = w_string_piece("foo").baseName().asWString();
  ok(str == "foo", "basename of foo is foo: %s", str.c_str());

  str = w_string_piece("foo\\bar").baseName().asWString();
#ifdef _WIN32
  ok(str == "bar", "basename of foo\\bar is bar: %s", str.c_str());
#else
  ok(str == "foo\\bar", "basename of foo\\bar is foo\\bar: %s", str.c_str());
#endif

  str = w_string_piece("foo\\bar").dirName().asWString();
#ifdef _WIN32
  ok(str == "foo", "dirname of foo\\bar is foo: %s", str.c_str());
#else
  ok(str == "", "dirname of foo\\bar is nothing: %s", str.c_str());
#endif

#ifdef _WIN32
  w_string_piece winFoo("C:\\foo");

  str = winFoo.baseName().asWString();
  ok(str == "foo", "basename of winfoo is %s", str.c_str());

  str = winFoo.dirName().asWString();
  ok(str == "C:\\", "dirname of winfoo is %s", str.c_str());

  str = winFoo.dirName().dirName().asWString();
  ok(str == "C:\\", "dirname of dirname winfoo is %s", str.c_str());
#endif

  // This is testing that we don't walk off the end of the string.
  // We had a bug where if the buffer had a slash as the character
  // after the end of the string, baseName and dirName could incorrectly
  // match that position and trigger a string range check.
  // The endSlash string below has 7 characters, with the 8th byte
  // as a slash to trigger this condition.
  w_string_piece endSlash("dir/foo/", 7);
  str = endSlash.baseName().asWString();
  ok(str == "foo", "basename is %s", str.c_str());
  str = endSlash.dirName().asWString();
  ok(str == "dir", "dirname is %s", str.c_str());
}

void test_operator() {
  ok(w_string_piece("a") < w_string_piece("b"), "a < b");
  ok(w_string_piece("a") < w_string_piece("ba"), "a < ba");
  ok(w_string_piece("aa") < w_string_piece("b"), "aa < b");
  ok(!(w_string_piece("b") < w_string_piece("a")), "b not < a");
  ok(!(w_string_piece("a") < w_string_piece("a")), "a not < a");
  ok(w_string_piece("A") < w_string_piece("a"), "A < a");
}

void test_split() {
  {
    std::vector<std::string> expected{"a", "b", "c"};
    std::vector<std::string> result;
    w_string_piece("a:b:c").split(result, ':');

    ok(expected == result, "split ok");
  }

  {
    std::vector<w_string> expected{"a", "b", "c"};
    std::vector<w_string> result;
    w_string_piece("a:b:c").split(result, ':');

    ok(expected == result, "split ok (w_string)");
  }

  {
    std::vector<std::string> expected{"a", "b", "c"};
    std::vector<std::string> result;
    w_string_piece("a:b:c:").split(result, ':');

    ok(expected == result, "split doesn't create empty last element");
  }

  {
    std::vector<std::string> expected{"a", "b", "", "c"};
    std::vector<std::string> result;
    w_string_piece("a:b::c:").split(result, ':');

    ok(expected == result, "split does create empty element");
  }

  {
    std::vector<std::string> result;
    w_string_piece().split(result, ':');
    ok(result.size() == 0, "split as 0 elements, got %d", int(result.size()));

    w_string_piece(w_string()).split(result, ':');
    ok(result.size() == 0, "split as 0 elements, got %d", int(result.size()));

    w_string_piece(w_string(nullptr)).split(result, ':');
    ok(result.size() == 0, "split as 0 elements, got %d", int(result.size()));
  }
}

void test_path_equal() {
  ok(w_string_piece("/foo/bar").pathIsEqual("/foo/bar"), "/foo/bar");
  ok(!w_string_piece("/foo/bar").pathIsEqual("/Foo/bar"), "/foo/bar");
#ifdef _WIN32
  ok(w_string_piece("c:/foo/bar").pathIsEqual("C:/foo/bar"),
     "allow different case for drive letter only c:/foo/bar");
  ok(w_string_piece("c:/foo\\bar").pathIsEqual("C:/foo/bar"),
     "allow different slashes c:/foo\\bar");
  ok(!w_string_piece("c:/Foo/bar").pathIsEqual("C:/foo/bar"),
     "strict case in the other positions c:/Foo/bar");
#endif
}

int main(int, char**) {
  plan_tests(
      124
#ifdef _WIN32
      + 6
#endif
      );
  test_integrals();
  test_strings();
  test_pointers();
  test_double();
  test_concat();
  test_lowercase_suffix();
  test_string_piece_suffix();
  test_string_piece_lowercase_suffix();
  test_to();
  test_path_cat();
  test_basename_dirname();
  test_operator();
  test_split();
  test_path_equal();

  return exit_status();
}
