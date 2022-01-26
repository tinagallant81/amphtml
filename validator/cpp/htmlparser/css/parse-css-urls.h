// This code is a stripped-down CSS parser that has enough remaining to support
// extracting URLs in a CSS stylesheet and identifying if those URLs represent
// images or fonts.
//
// This is parse-css.h/cc stripped down. The expected usage is to call
// SegmentCSS and use the resulting segments to modify URLs found in the input
// CSS. See search/amphtml/transformers/external_url_rewrite_transformer.cc for
// the primary use case of this library.

#ifndef CPP_HTMLPARSER_CSS_PARSE_CSS_URLS_H_
#define CPP_HTMLPARSER_CSS_PARSE_CSS_URLS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/variant.h"
#include "cpp/htmlparser/css/parse-css-urls.pb.h"

namespace htmlparser::css::url {
// Implements 3.3. Preprocessing the input stream.
// http://www.w3.org/TR/css-syntax-3/#input-preprocessing
void Preprocess(std::vector<char32_t>* codepoints);

// Superclass for all objects generated by this lexer/parser
// combination. For each subclass, a distinct TokenType::Code (see
// proto buffer) is passed in. The C++ code uses this where the Javascript
// uses instanceof checks.
class Token {
 public:
  explicit Token(TokenType::Code type) : type_(type), pos_(0) {}
  virtual ~Token() = default;

  TokenType::Code Type() const { return type_; }

  virtual const std::string& StringValue() const;

  // Note that |pos()| is the position after performing preprocessing, that is,
  // if you have '\r' in your input then this routine will yield the "wrong"
  // positions. Call quality_dni::parse_css_urls::Preprocess or
  // strings::CleanStringLineEndings on your input to fix the problem.
  void set_pos(int32_t pos) { pos_ = pos; }
  int32_t pos() const { return pos_; }

  // Propagates the start position of |this| to |other|.
  void CopyStartPositionTo(Token* other) const {
    other->set_pos(pos_);
  }

 private:
  TokenType::Code type_;
  int32_t pos_;
};

class WhitespaceToken : public Token {
 public:
  WhitespaceToken() : Token(TokenType::WHITESPACE) {}
};

class CDCToken : public Token {
 public:
  CDCToken() : Token(TokenType::CDC) {}
};

class CDOToken : public Token {
 public:
  CDOToken() : Token(TokenType::CDO) {}
};

class ColonToken : public Token {
 public:
  ColonToken() : Token(TokenType::COLON) {}
};

class SemicolonToken : public Token {
 public:
  SemicolonToken() : Token(TokenType::SEMICOLON) {}
};

class CommaToken : public Token {
 public:
  CommaToken() : Token(TokenType::COMMA) {}
};

// Grouping tokens are {}, [], ().
class GroupingToken : public Token {
 public:
  explicit GroupingToken(TokenType::Code type) : Token(type) {}
};

class OpenCurlyToken : public GroupingToken {
 public:
  OpenCurlyToken() : GroupingToken(TokenType::OPEN_CURLY) {}
};

class CloseCurlyToken : public GroupingToken {
 public:
  CloseCurlyToken() : GroupingToken(TokenType::CLOSE_CURLY) {}
};

class OpenSquareToken : public GroupingToken {
 public:
  OpenSquareToken() : GroupingToken(TokenType::OPEN_SQUARE) {}
};

class CloseSquareToken : public GroupingToken {
 public:
  CloseSquareToken() : GroupingToken(TokenType::CLOSE_SQUARE) {}
};

class OpenParenToken : public GroupingToken {
 public:
  OpenParenToken() : GroupingToken(TokenType::OPEN_PAREN) {}
};

class CloseParenToken : public GroupingToken {
 public:
  CloseParenToken() : GroupingToken(TokenType::CLOSE_PAREN) {}
};

class IncludeMatchToken : public Token {
 public:
  IncludeMatchToken() : Token(TokenType::INCLUDE_MATCH) {}
};

class DashMatchToken : public Token {
 public:
  DashMatchToken() : Token(TokenType::DASH_MATCH) {}
};

class PrefixMatchToken : public Token {
 public:
  PrefixMatchToken() : Token(TokenType::PREFIX_MATCH) {}
};

class SuffixMatchToken : public Token {
 public:
  SuffixMatchToken() : Token(TokenType::SUFFIX_MATCH) {}
};

class SubstringMatchToken : public Token {
 public:
  SubstringMatchToken() : Token(TokenType::SUBSTRING_MATCH) {}
};

class ColumnToken : public Token {
 public:
  ColumnToken() : Token(TokenType::COLUMN) {}
};

class EOFToken : public Token {
 public:
  EOFToken() : Token(TokenType::EOF_TOKEN) {}
};

class DelimToken : public Token {
 public:
  DelimToken() : Token(TokenType::DELIM) {}
};

class StringValuedToken : public Token {
 public:
  StringValuedToken(const std::string& value, TokenType::Code type)
      : Token(type), value_(value) {}

  const std::string& StringValue() const override;

 private:
  std::string value_;
};

class IdentToken : public StringValuedToken {
 public:
  explicit IdentToken(const std::string& val)
      : StringValuedToken(val, TokenType::IDENT) {}
};

class FunctionToken : public StringValuedToken {
 public:
  explicit FunctionToken(const std::string& val)
      : StringValuedToken(val, TokenType::FUNCTION_TOKEN) {}
};

class AtKeywordToken : public StringValuedToken {
 public:
  explicit AtKeywordToken(const std::string& val)
      : StringValuedToken(val, TokenType::AT_KEYWORD) {}
};

class HashToken : public StringValuedToken {
 public:
  explicit HashToken(const std::string& val)
      : StringValuedToken(val, TokenType::HASH) {}
};

class StringToken : public StringValuedToken {
 public:
  explicit StringToken(const std::string& val)
      : StringValuedToken(val, TokenType::STRING) {}
};

class URLToken : public StringValuedToken {
 public:
  explicit URLToken(const std::string& val)
      : StringValuedToken(val, TokenType::URL) {}
};

class NumberToken : public Token {
 public:
  NumberToken() : Token(TokenType::NUMBER) {}
};

class PercentageToken : public Token {
 public:
  explicit PercentageToken() : Token(TokenType::PERCENTAGE) {}
};

class DimensionToken : public Token {
 public:
  DimensionToken() : Token(TokenType::DIMENSION) {}
};

class ErrorToken : public Token {
 public:
  ErrorToken() : Token(TokenType::ERROR) {}
};

// Tokenizes the provided input string.
std::vector<std::unique_ptr<Token>> Tokenize(
    const std::vector<char32_t>& str_in,
    std::vector<std::unique_ptr<ErrorToken>>* errors);

struct CssSegment {
  enum Type {
    // If |type| is BYTES, then |utf8_data| holds a utf8-encoded byte
    // array which is part of the CSS stylesheet. If included in the output,
    // it will need to be emitted as is.
    BYTES = 0,
    // If |type| is IMAGE_URL, then |utf8_data| holds a URL for an image.
    // When included in CSS output, this URL must be enclosed into a
    // url token or function token with name 'url'. E.g., the following is
    // OK: StrCat("url(", utf8_data, ")"); as is this (with quotes):
    // StrCat("url('", utf8_data, "')");
    IMAGE_URL = 1,
    // Similar to IMAGE_URL case, but for fonts.
    OTHER_URL = 2,
  };
  Type type;
  std::string utf8_data;
};

// This function chops a style sheet into |segments|. Each segment is
// either a UTF8 encoded byte string, or an image or other URL (in
// practice a font).  This can be used to modify the URLs to point at
// a CDN.  Note that when combining the segments back to a stylesheet,
// the client code must emit url() around URLs. This is done so that
// client code can choose the quote character as in
// url("http://foo.com") or url('http://foo.com/') or even leave out
// the quote character as in url(http://foo.com/). Note that CSS supports
// escaping quote characters within a string by prefixing with a backslash,
// so " inside a URL may be written as \".
bool SegmentCss(const std::string& utf8_css,
                std::vector<CssSegment>* segments);

}  // namespace htmlparser::css::url

#endif  // CPP_HTMLPARSER_CSS_PARSE_CSS_URLS_H_
