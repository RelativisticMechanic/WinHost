/*
    Maddy - A C++ MD -> HTML Parser written by Petra Baranski.
    See: https://github.com/progsource/maddy
    Licensed under the MIT License.
    All copyrights are property of their original owners.

    NOTES FROM S, GAUTAM:
    I have merged the header only library into a single file and minified it.
    maddy version latest as of (11-08-2023, 16:39 IST)
*/

#pragma once
#include <memory>
#include <functional>
#include <string>
#include <sstream>
#include <string>
#include <regex>
#include <cctype>
#include <algorithm>
#include <stdint.h>
namespace maddy {
namespace types {
enum PARSER_TYPE : uint32_t
{
 NONE = 0,
 BREAKLINE_PARSER = 0b1,
 CHECKLIST_PARSER = 0b10,
 CODE_BLOCK_PARSER = 0b100,
 EMPHASIZED_PARSER = 0b1000,
 HEADLINE_PARSER = 0b10000,
 HORIZONTAL_LINE_PARSER = 0b100000,
 HTML_PARSER = 0b1000000,
 IMAGE_PARSER = 0b10000000,
 INLINE_CODE_PARSER = 0b100000000,
 ITALIC_PARSER = 0b1000000000,
 LINK_PARSER = 0b10000000000,
 ORDERED_LIST_PARSER = 0b100000000000,
 PARAGRAPH_PARSER = 0b1000000000000,
 QUOTE_PARSER = 0b10000000000000,
 STRIKETHROUGH_PARSER = 0b100000000000000,
 STRONG_PARSER = 0b1000000000000000,
 TABLE_PARSER = 0b10000000000000000,
 UNORDERED_LIST_PARSER = 0b100000000000000000,
 LATEX_BLOCK_PARSER = 0b1000000000000000000,
 DEFAULT = 0b0111111111110111111,
 ALL = 0b1111111111111111111,
};
} struct ParserConfig
{
 bool isEmphasizedParserEnabled;
 bool isHTMLWrappedInParagraph;
 uint32_t enabledParsers;
 ParserConfig()
 : isEmphasizedParserEnabled(true)
 , isHTMLWrappedInParagraph(true)
 , enabledParsers(maddy::types::DEFAULT)
 {}
}; } namespace maddy {
class BlockParser
{
public:
 BlockParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : result("", std::ios_base::ate | std::ios_base::in | std::ios_base::out)
 , childParser(nullptr)
 , parseLineCallback(parseLineCallback)
 , getBlockParserForLineCallback(getBlockParserForLineCallback)
 {}
 virtual ~BlockParser() {}
 virtual void
 AddLine(std::string& line)
 {
 this->parseBlock(line);
 if (this->isInlineBlockAllowed() && !this->childParser)
 {
 this->childParser = this->getBlockParserForLine(line);
 }
 if (this->childParser)
 {
 this->childParser->AddLine(line);
 if (this->childParser->IsFinished())
 {
 this->result << this->childParser->GetResult().str();
 this->childParser = nullptr;
 }
 return;
 }
 if (this->isLineParserAllowed())
 {
 this->parseLine(line);
 }
 this->result << line;
 }
 virtual bool IsFinished() const = 0;
 std::stringstream&
 GetResult()
 {
 return this->result;
 }
 void
 Clear()
 {
 this->result.str("");
 }
protected:
 std::stringstream result;
 std::shared_ptr<BlockParser> childParser;
 virtual bool isInlineBlockAllowed() const = 0;
 virtual bool isLineParserAllowed() const = 0;
 virtual void parseBlock(std::string& line) = 0;
 void
 parseLine(std::string& line)
 {
 if (parseLineCallback)
 {
 parseLineCallback(line);
 }
 }
 uint32_t
 getIndentationWidth(const std::string& line) const
 {
 bool hasMetNonSpace = false;
 uint32_t indentation = static_cast<uint32_t>(
 std::count_if(
 line.begin(),
 line.end(),
 [&hasMetNonSpace](unsigned char c)
 {
 if (hasMetNonSpace)
 {
 return false;
 }
 if (std::isspace(c))
 {
 return true;
 }
 hasMetNonSpace = true;
 return false;
 }
 )
 );
 return indentation;
 }
 std::shared_ptr<BlockParser>
 getBlockParserForLine(const std::string& line)
 {
 if (getBlockParserForLineCallback)
 {
 return getBlockParserForLineCallback(line);
 }
 return nullptr;
 }
private:
 std::function<void(std::string&)> parseLineCallback;
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback;
}; } namespace maddy {
class ChecklistParser : public BlockParser
{
public:
 ChecklistParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::regex re(R"(^- \[[x| ]\] .*)");
 return std::regex_match(line, re);
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return true;
 }
 bool
 isLineParserAllowed() const override
 {
 return true;
 }
 void
 parseBlock(std::string& line) override
 {
 bool isStartOfNewListItem = IsStartingLine(line);
 uint32_t indentation = getIndentationWidth(line);
 static std::regex lineRegex("^(- )");
 line = std::regex_replace(line, lineRegex, "");
 static std::regex emptyBoxRegex(R"(^\[ \])");
 static std::string emptyBoxReplacement = "<input type=\"checkbox\"/>";
 line = std::regex_replace(line, emptyBoxRegex, emptyBoxReplacement);
 static std::regex boxRegex(R"(^\[x\])");
 static std::string boxReplacement = "<input type=\"checkbox\" checked=\"checked\"/>";
 line = std::regex_replace(line, boxRegex, boxReplacement);
 if (!this->isStarted)
 {
 line = "<ul class=\"checklist\"><li><label>" + line;
 this->isStarted = true;
 return;
 }
 if (indentation >= 2)
 {
 line = line.substr(2);
 return;
 }
 if (
 line.empty() ||
 line.find("</label></li><li><label>") != std::string::npos ||
 line.find("</label></li></ul>") != std::string::npos
 )
 {
 line = "</label></li></ul>" + line;
 this->isFinished = true;
 return;
 }
 if (isStartOfNewListItem)
 {
 line = "</label></li><li><label>" + line;
 }
 }
private:
 bool isStarted;
 bool isFinished;
}; } namespace maddy {
class CodeBlockParser : public BlockParser
{
public:
 CodeBlockParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::regex re("^(?:`){3}(.*)$");
 return std::regex_match(line, re);
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return false;
 }
 bool
 isLineParserAllowed() const override
 {
 return false;
 }
 void
 parseBlock(std::string& line) override
 {
 if (line == "```")
 {
 if (!this->isStarted)
 {
 line = "<pre><code>\n";
 this->isStarted = true;
 this->isFinished = false;
 return;
 }
 else
 {
 line = "</code></pre>";
 this->isFinished = true;
 this->isStarted = false;
 return;
 }
 }
 else if (!this->isStarted && line.substr(0, 3) == "```")
 {
 line = "<pre class=\"" + line.substr(3) + "\"><code>\n";
 this->isStarted = true;
 this->isFinished = false;
 return;
 }
 line += "\n";
 }
private:
 bool isStarted;
 bool isFinished;
}; } namespace maddy {
class HeadlineParser : public BlockParser
{
public:
 HeadlineParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::regex re("^(?:#){1,6} (.*)");
 return std::regex_match(line, re);
 }
 bool
 IsFinished() const override
 {
 return true;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return false;
 }
 bool
 isLineParserAllowed() const override
 {
 return false;
 }
 void
 parseBlock(std::string& line) override
 {
 static std::vector<std::regex> hlRegex = {
 std::regex("^# (.*)")
 , std::regex("^(?:#){2} (.*)")
 , std::regex("^(?:#){3} (.*)")
 , std::regex("^(?:#){4} (.*)")
 , std::regex("^(?:#){5} (.*)")
 , std::regex("^(?:#){6} (.*)")
 };
 static std::vector<std::string> hlReplacement = {
 "<h1>$1</h1>"
 , "<h2>$1</h2>"
 , "<h3>$1</h3>"
 , "<h4>$1</h4>"
 , "<h5>$1</h5>"
 , "<h6>$1</h6>"
 };
 for (uint8_t i = 0; i < 6; ++i)
 {
 line = std::regex_replace(line, hlRegex[i], hlReplacement[i]);
 }
 }
}; } namespace maddy {
class HorizontalLineParser : public BlockParser
{
public:
 HorizontalLineParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , lineRegex("^---$")
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::regex re("^---$");
 return std::regex_match(line, re);
 }
 bool
 IsFinished() const override
 {
 return true;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return false;
 }
 bool
 isLineParserAllowed() const override
 {
 return false;
 }
 void
 parseBlock(std::string& line) override
 {
 static std::string replacement = "<hr/>";
 line = std::regex_replace(line, lineRegex, replacement);
 }
private:
 std::regex lineRegex;
}; } namespace maddy {
class HtmlParser : public BlockParser
{
public:
 HtmlParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 , isGreaterThanFound(false)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 return line[0] == '<';
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return false;
 }
 bool
 isLineParserAllowed() const override
 {
 return false;
 }
 void
 parseBlock(std::string& line) override
 {
 if (!this->isStarted)
 {
 this->isStarted = true;
 }
 if (!line.empty() && line[line.size() - 1] == '>')
 {
 this->isGreaterThanFound = true;
 return;
 }
 if (line.empty() && this->isGreaterThanFound)
 {
 this->isFinished = true;
 return;
 }
 if (!line.empty() && this->isGreaterThanFound)
 {
 this->isGreaterThanFound = false;
 }
 if (!line.empty())
 {
 line += " ";
 }
 }
private:
 bool isStarted;
 bool isFinished;
 bool isGreaterThanFound;
}; } namespace maddy {
class LatexBlockParser : public BlockParser
{
public:
 LatexBlockParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::regex re(R"(^(?:\$){2}(.*)$)");
 return std::regex_match(line, re);
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return false;
 }
 bool
 isLineParserAllowed() const override
 {
 return false;
 }
 void
 parseBlock(std::string& line) override
 {
 if (!this->isStarted && line.substr(0, 2) == "$$")
 {
 this->isStarted = true;
 this->isFinished = false;
 }
 if (this->isStarted && !this->isFinished && line.size() > 1 && line.substr(line.size() - 2, 2) == "$$")
 {
 this->isFinished = true;
 this->isStarted = false;
 }
 line += "\n";
 }
private:
 bool isStarted;
 bool isFinished;
}; } namespace maddy {
class OrderedListParser : public BlockParser
{
public:
 OrderedListParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::regex re("^1\\. .*");
 return std::regex_match(line, re);
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return true;
 }
 bool
 isLineParserAllowed() const override
 {
 return true;
 }
 void
 parseBlock(std::string& line) override
 {
 bool isStartOfNewListItem = this->isStartOfNewListItem(line);
 uint32_t indentation = getIndentationWidth(line);
 static std::regex orderedlineRegex(R"(^[1-9]+[0-9]*\. )");
 line = std::regex_replace(line, orderedlineRegex, "");
 static std::regex unorderedlineRegex(R"(^\* )");
 line = std::regex_replace(line, unorderedlineRegex, "");
 if (!this->isStarted)
 {
 line = "<ol><li>" + line;
 this->isStarted = true;
 return;
 }
 if (indentation >= 2)
 {
 line = line.substr(2);
 return;
 }
 if (
 line.empty() ||
 line.find("</li><li>") != std::string::npos ||
 line.find("</li></ol>") != std::string::npos ||
 line.find("</li></ul>") != std::string::npos
 )
 {
 line = "</li></ol>" + line;
 this->isFinished = true;
 return;
 }
 if (isStartOfNewListItem)
 {
 line = "</li><li>" + line;
 }
 }
private:
 bool isStarted;
 bool isFinished;
 bool
 isStartOfNewListItem(const std::string& line) const
 {
 static std::regex re(R"(^(?:[1-9]+[0-9]*\. |\* ).*)");
 return std::regex_match(line, re);
 }
}; } namespace maddy {
class ParagraphParser : public BlockParser
{
public:
 ParagraphParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback,
 bool isEnabled
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 , isEnabled(isEnabled)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 return !line.empty();
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return false;
 }
 bool
 isLineParserAllowed() const override
 {
 return true;
 }
 void
 parseBlock(std::string& line) override
 {
 if (this->isEnabled && !this->isStarted)
 {
 line = "<p>" + line + " ";
 this->isStarted = true;
 return;
 }
 else if (!this->isEnabled && !this->isStarted)
 {
 line += " ";
 this->isStarted = true;
 return;
 }
 if (this->isEnabled && line.empty())
 {
 line += "</p>";
 this->isFinished = true;
 return;
 }
 else if (!this->isEnabled && line.empty())
 {
 line += "<br/>";
 this->isFinished = true;
 return;
 }
 line += " ";
 }
private:
 bool isStarted;
 bool isFinished;
 bool isEnabled;
}; } namespace maddy {
class QuoteParser : public BlockParser
{
public:
 QuoteParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::regex re(R"(^\>.*)");
 return std::regex_match(line, re);
 }
 void
 AddLine(std::string& line) override
 {
 if (!this->isStarted)
 {
 this->result << "<blockquote>";
 this->isStarted = true;
 }
 bool finish = false;
 if (line.empty())
 {
 finish = true;
 }
 this->parseBlock(line);
 if (this->isInlineBlockAllowed() && !this->childParser)
 {
 this->childParser = this->getBlockParserForLine(line);
 }
 if (this->childParser)
 {
 this->childParser->AddLine(line);
 if (this->childParser->IsFinished())
 {
 this->result << this->childParser->GetResult().str();
 this->childParser = nullptr;
 }
 return;
 }
 if (this->isLineParserAllowed())
 {
 this->parseLine(line);
 }
 if (finish)
 {
 this->result << "</blockquote>";
 this->isFinished = true;
 }
 this->result << line;
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return true;
 }
 bool
 isLineParserAllowed() const override
 {
 return true;
 }
 void
 parseBlock(std::string& line) override
 {
 static std::regex lineRegexWithSpace(R"(^\> )");
 line = std::regex_replace(line, lineRegexWithSpace, "");
 static std::regex lineRegexWithoutSpace(R"(^\>)");
 line = std::regex_replace(line, lineRegexWithoutSpace, "");
 if (!line.empty())
 {
 line += " ";
 }
 }
private:
 bool isStarted;
 bool isFinished;
}; } namespace maddy {
class TableParser : public BlockParser
{
public:
 TableParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 , currentBlock(0)
 , currentRow(0)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::string matchString("|table>");
 return line == matchString;
 }
 void
 AddLine(std::string& line) override
 {
 if (!this->isStarted && line == "|table>")
 {
 this->isStarted = true;
 return;
 }
 if (this->isStarted)
 {
 if (line == "- | - | -")
 {
 ++this->currentBlock;
 this->currentRow = 0;
 return;
 }
 if (line == "|<table")
 {
 static std::string emptyLine = "";
 this->parseBlock(emptyLine);
 this->isFinished = true;
 return;
 }
 if (this->table.size() < this->currentBlock + 1)
 {
 this->table.push_back(std::vector<std::vector<std::string>>());
 }
 this->table[this->currentBlock].push_back(std::vector<std::string>());
 std::string segment;
 std::stringstream streamToSplit(line);
 while (std::getline(streamToSplit, segment, '|'))
 {
 this->parseLine(segment);
 this->table[this->currentBlock][this->currentRow].push_back(segment);
 }
 ++this->currentRow;
 }
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return false;
 }
 bool
 isLineParserAllowed() const override
 {
 return true;
 }
 void
 parseBlock(std::string&) override
 {
 result << "<table>";
 bool hasHeader = false;
 bool hasFooter = false;
 bool isFirstBlock = true;
 uint32_t currentBlockNumber = 0;
 if (this->table.size() > 1)
 {
 hasHeader = true;
 }
 if (this->table.size() >= 3)
 {
 hasFooter = true;
 }
 for (const std::vector<std::vector<std::string>>& block : this->table)
 {
 bool isInHeader = false;
 bool isInFooter = false;
 ++currentBlockNumber;
 if (hasHeader && isFirstBlock)
 {
 result << "<thead>";
 isInHeader = true;
 }
 else if (hasFooter && currentBlockNumber == this->table.size())
 {
 result << "<tfoot>";
 isInFooter = true;
 }
 else
 {
 result << "<tbody>";
 }
 for (const std::vector<std::string>& row : block)
 {
 result << "<tr>";
 for (const std::string& column : row)
 {
 if (isInHeader)
 {
 result << "<th>";
 }
 else
 {
 result << "<td>";
 }
 result << column;
 if (isInHeader)
 {
 result << "</th>";
 }
 else
 {
 result << "</td>";
 }
 }
 result << "</tr>";
 }
 if (isInHeader)
 {
 result << "</thead>";
 }
 else if (isInFooter)
 {
 result << "</tfoot>";
 }
 else
 {
 result << "</tbody>";
 }
 isFirstBlock = false;
 }
 result << "</table>";
 }
private:
 bool isStarted;
 bool isFinished;
 uint32_t currentBlock;
 uint32_t currentRow;
 std::vector<std::vector<std::vector<std::string>>> table;
}; } namespace maddy {
class UnorderedListParser : public BlockParser
{
public:
 UnorderedListParser(
 std::function<void(std::string&)> parseLineCallback,
 std::function<std::shared_ptr<BlockParser>(const std::string& line)> getBlockParserForLineCallback
 )
 : BlockParser(parseLineCallback, getBlockParserForLineCallback)
 , isStarted(false)
 , isFinished(false)
 {}
 static bool
 IsStartingLine(const std::string& line)
 {
 static std::regex re("^[+*-] .*");
 return std::regex_match(line, re);
 }
 bool
 IsFinished() const override
 {
 return this->isFinished;
 }
protected:
 bool
 isInlineBlockAllowed() const override
 {
 return true;
 }
 bool
 isLineParserAllowed() const override
 {
 return true;
 }
 void
 parseBlock(std::string& line) override
 {
 bool isStartOfNewListItem = IsStartingLine(line);
 uint32_t indentation = getIndentationWidth(line);
 static std::regex lineRegex("^([+*-] )");
 line = std::regex_replace(line, lineRegex, "");
 if (!this->isStarted)
 {
 line = "<ul><li>" + line;
 this->isStarted = true;
 return;
 }
 if (indentation >= 2)
 {
 line = line.substr(2);
 return;
 }
 if (
 line.empty() ||
 line.find("</li><li>") != std::string::npos ||
 line.find("</li></ol>") != std::string::npos ||
 line.find("</li></ul>") != std::string::npos
 )
 {
 line = "</li></ul>" + line;
 this->isFinished = true;
 return;
 }
 if (isStartOfNewListItem)
 {
 line = "</li><li>" + line;
 }
 }
private:
 bool isStarted;
 bool isFinished;
}; } namespace maddy {
class LineParser
{
public:
 virtual ~LineParser() {}
 virtual void Parse(std::string& line) = 0;
}; } namespace maddy {
class BreakLineParser : public LineParser
{
public:
 void
 Parse(std::string& line) override
 {
 static std::regex re(R"((\r\n|\r))");
 static std::string replacement = "<br>";
 line = std::regex_replace(line, re, replacement);
 }
}; } namespace maddy {
class EmphasizedParser : public LineParser
{
public:
 void
 Parse(std::string& line) override
 {
 static std::regex re(R"((?!.*`.*|.*<code>.*)_(?!.*`.*|.*<\/code>.*)([^_]*)_(?!.*`.*|.*<\/code>.*))");
 static std::string replacement = "<em>$1</em>";
 line = std::regex_replace(line, re, replacement);
 }
}; } namespace maddy {
class ImageParser : public LineParser
{
public:
 void
 Parse(std::string& line) override
 {
 static std::regex re(R"(\!\[([^\]]*)\]\(([^\]]*)\))");
 static std::string replacement = "<img src=\"$2\" alt=\"$1\"/>";
 line = std::regex_replace(line, re, replacement);
 }
}; } namespace maddy {
class InlineCodeParser : public LineParser
{
public:
 void
 Parse(std::string& line) override
 {
 static std::regex re("`([^`]*)`");
 static std::string replacement = "<code>$1</code>";
 line = std::regex_replace(line, re, replacement);
 }
}; } namespace maddy {
class ItalicParser : public LineParser
{
public:
 void
 Parse(std::string& line) override
 {
 static std::regex re(R"((?!.*`.*|.*<code>.*)\*(?!.*`.*|.*<\/code>.*)([^\*]*)\*(?!.*`.*|.*<\/code>.*))");
 static std::string replacement = "<i>$1</i>";
 line = std::regex_replace(line, re, replacement);
 }
}; } namespace maddy {
class LinkParser : public LineParser
{
public:
 void
 Parse(std::string& line) override
 {
 static std::regex re(R"(\[([^\]]*)\]\(([^\]]*)\))");
 static std::string replacement = "<a href=\"$2\">$1</a>";
 line = std::regex_replace(line, re, replacement);
 }
}; } namespace maddy {
class StrikeThroughParser : public LineParser
{
public:
 void
 Parse(std::string& line) override
 {
 static std::regex re(R"((?!.*`.*|.*<code>.*)\~\~(?!.*`.*|.*<\/code>.*)([^\~]*)\~\~(?!.*`.*|.*<\/code>.*))");
 static std::string replacement = "<s>$1</s>";
 line = std::regex_replace(line, re, replacement);
 }
}; } namespace maddy {
class StrongParser : public LineParser
{
public:
 void
 Parse(std::string& line) override
 {
 static std::vector<std::regex> res
 {
 std::regex{R"((?!.*`.*|.*<code>.*)\*\*(?!.*`.*|.*<\/code>.*)([^\*\*]*)\*\*(?!.*`.*|.*<\/code>.*))"},
 std::regex{R"((?!.*`.*|.*<code>.*)__(?!.*`.*|.*<\/code>.*)([^__]*)__(?!.*`.*|.*<\/code>.*))"}
 };
 static std::string replacement = "<strong>$1</strong>";
 for (const auto& re : res)
 {
 line = std::regex_replace(line, re, replacement);
 }
 }
}; } namespace maddy {
class Parser
{
public:
 static const std::string& version() { static const std::string v = "1.2.1"; return v; }
 Parser(std::shared_ptr<ParserConfig> config = nullptr)
 : config(config)
 {
 if (this->config && !this->config->isEmphasizedParserEnabled)
 {
 this->config->enabledParsers &= ~maddy::types::EMPHASIZED_PARSER;
 }
 if (this->config && !this->config->isHTMLWrappedInParagraph)
 {
 this->config->enabledParsers |= maddy::types::HTML_PARSER;
 }
 if (
 !this->config ||
 (this->config->enabledParsers & maddy::types::BREAKLINE_PARSER) != 0
 )
 {
 this->breakLineParser = std::make_shared<BreakLineParser>();
 }
 if (
 !this->config ||
 (this->config->enabledParsers & maddy::types::EMPHASIZED_PARSER) != 0
 )
 {
 this->emphasizedParser = std::make_shared<EmphasizedParser>();
 }
 if (
 !this->config ||
 (this->config->enabledParsers & maddy::types::IMAGE_PARSER) != 0
 )
 {
 this->imageParser = std::make_shared<ImageParser>();
 }
 if (
 !this->config ||
 (this->config->enabledParsers & maddy::types::INLINE_CODE_PARSER) != 0
 )
 {
 this->inlineCodeParser = std::make_shared<InlineCodeParser>();
 }
 if (
 !this->config ||
 (this->config->enabledParsers & maddy::types::ITALIC_PARSER) != 0
 )
 {
 this->italicParser = std::make_shared<ItalicParser>();
 }
 if (
 !this->config ||
 (this->config->enabledParsers & maddy::types::LINK_PARSER) != 0
 )
 {
 this->linkParser = std::make_shared<LinkParser>();
 }
 if (
 !this->config ||
 (this->config->enabledParsers & maddy::types::STRIKETHROUGH_PARSER) != 0
 )
 {
 this->strikeThroughParser = std::make_shared<StrikeThroughParser>();
 }
 if (
 !this->config ||
 (this->config->enabledParsers & maddy::types::STRONG_PARSER) != 0
 )
 {
 this->strongParser = std::make_shared<StrongParser>();
 }
 }
 std::string
 Parse(std::istream& markdown) const
 {
 std::string result = "";
 std::shared_ptr<BlockParser> currentBlockParser = nullptr;
 for (std::string line; std::getline(markdown, line);)
 {
 if (!currentBlockParser)
 {
 currentBlockParser = getBlockParserForLine(line);
 }
 if (currentBlockParser)
 {
 currentBlockParser->AddLine(line);
 if (currentBlockParser->IsFinished())
 {
 result += currentBlockParser->GetResult().str();
 currentBlockParser = nullptr;
 }
 }
 }
 if (currentBlockParser)
 {
 std::string emptyLine = "";
 currentBlockParser->AddLine(emptyLine);
 if (currentBlockParser->IsFinished())
 {
 result += currentBlockParser->GetResult().str();
 currentBlockParser = nullptr;
 }
 }
 return result;
 }
private:
 std::shared_ptr<ParserConfig> config;
 std::shared_ptr<BreakLineParser> breakLineParser;
 std::shared_ptr<EmphasizedParser> emphasizedParser;
 std::shared_ptr<ImageParser> imageParser;
 std::shared_ptr<InlineCodeParser> inlineCodeParser;
 std::shared_ptr<ItalicParser> italicParser;
 std::shared_ptr<LinkParser> linkParser;
 std::shared_ptr<StrikeThroughParser> strikeThroughParser;
 std::shared_ptr<StrongParser> strongParser;
 void
 runLineParser(std::string& line) const
 {
 if (this->imageParser)
 {
 this->imageParser->Parse(line);
 }
 if (this->linkParser)
 {
 this->linkParser->Parse(line);
 }
 if (this->strongParser)
 {
 this->strongParser->Parse(line);
 }
 if (this->emphasizedParser)
 {
 this->emphasizedParser->Parse(line);
 }
 if (this->strikeThroughParser)
 {
 this->strikeThroughParser->Parse(line);
 }
 if (this->inlineCodeParser)
 {
 this->inlineCodeParser->Parse(line);
 }
 if (this->italicParser)
 {
 this->italicParser->Parse(line);
 }
 if (this->breakLineParser)
 {
 this->breakLineParser->Parse(line);
 }
 }
 std::shared_ptr<BlockParser>
 getBlockParserForLine(const std::string& line) const
 {
 std::shared_ptr<BlockParser> parser;
 if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::CODE_BLOCK_PARSER) != 0
 ) &&
 maddy::CodeBlockParser::IsStartingLine(line)
 )
 {
 parser = std::make_shared<maddy::CodeBlockParser>(
 nullptr,
 nullptr
 );
 }
 else if (
 this->config &&
 (this->config->enabledParsers & maddy::types::LATEX_BLOCK_PARSER) != 0 &&
 maddy::LatexBlockParser::IsStartingLine(line)
 )
 {
 parser = std::make_shared<LatexBlockParser>(
 nullptr,
 nullptr
 );
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::HEADLINE_PARSER) != 0
 ) &&
 maddy::HeadlineParser::IsStartingLine(line)
 )
 {
 parser = std::make_shared<maddy::HeadlineParser>(
 nullptr,
 nullptr
 );
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::HORIZONTAL_LINE_PARSER) != 0
 ) &&
 maddy::HorizontalLineParser::IsStartingLine(line)
 )
 {
 parser = std::make_shared<maddy::HorizontalLineParser>(
 nullptr,
 nullptr
 );
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::QUOTE_PARSER) != 0
 ) &&
 maddy::QuoteParser::IsStartingLine(line)
 )
 {
 parser = std::make_shared<maddy::QuoteParser>(
 [this](std::string& line){ this->runLineParser(line); },
 [this](const std::string& line){ return this->getBlockParserForLine(line); }
 );
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::TABLE_PARSER) != 0
 ) &&
 maddy::TableParser::IsStartingLine(line)
 )
 {
 parser = std::make_shared<maddy::TableParser>(
 [this](std::string& line){ this->runLineParser(line); },
 nullptr
 );
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::CHECKLIST_PARSER) != 0
 ) &&
 maddy::ChecklistParser::IsStartingLine(line)
 )
 {
 parser = this->createChecklistParser();
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::ORDERED_LIST_PARSER) != 0
 ) &&
 maddy::OrderedListParser::IsStartingLine(line)
 )
 {
 parser = this->createOrderedListParser();
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::UNORDERED_LIST_PARSER) != 0
 ) &&
 maddy::UnorderedListParser::IsStartingLine(line)
 )
 {
 parser = this->createUnorderedListParser();
 }
 else if (
 this->config &&
 (this->config->enabledParsers & maddy::types::HTML_PARSER) != 0 &&
 maddy::HtmlParser::IsStartingLine(line)
 )
 {
 parser = std::make_shared<maddy::HtmlParser>(nullptr, nullptr);
 }
 else if (
 maddy::ParagraphParser::IsStartingLine(line)
 )
 {
 parser = std::make_shared<maddy::ParagraphParser>(
 [this](std::string& line){ this->runLineParser(line); },
 nullptr,
 (!this->config || (this->config->enabledParsers & maddy::types::PARAGRAPH_PARSER) != 0)
 );
 }
 return parser;
 }
 std::shared_ptr<BlockParser>
 createChecklistParser() const
 {
 return std::make_shared<maddy::ChecklistParser>(
 [this](std::string& line){ this->runLineParser(line); },
 [this](const std::string& line)
 {
 std::shared_ptr<BlockParser> parser;
 if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::CHECKLIST_PARSER) != 0
 ) &&
 maddy::ChecklistParser::IsStartingLine(line)
 )
 {
 parser = this->createChecklistParser();
 }
 return parser;
 }
 );
 }
 std::shared_ptr<BlockParser>
 createOrderedListParser() const
 {
 return std::make_shared<maddy::OrderedListParser>(
 [this](std::string& line){ this->runLineParser(line); },
 [this](const std::string& line)
 {
 std::shared_ptr<BlockParser> parser;
 if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::ORDERED_LIST_PARSER) != 0
 ) &&
 maddy::OrderedListParser::IsStartingLine(line)
 )
 {
 parser = this->createOrderedListParser();
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::UNORDERED_LIST_PARSER) != 0
 ) &&
 maddy::UnorderedListParser::IsStartingLine(line)
 )
 {
 parser = this->createUnorderedListParser();
 }
 return parser;
 }
 );
 }
 std::shared_ptr<BlockParser>
 createUnorderedListParser() const
 {
 return std::make_shared<maddy::UnorderedListParser>(
 [this](std::string& line){ this->runLineParser(line); },
 [this](const std::string& line)
 {
 std::shared_ptr<BlockParser> parser;
 if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::ORDERED_LIST_PARSER) != 0
 ) &&
 maddy::OrderedListParser::IsStartingLine(line)
 )
 {
 parser = this->createOrderedListParser();
 }
 else if (
 (
 !this->config ||
 (this->config->enabledParsers & maddy::types::UNORDERED_LIST_PARSER) != 0
 ) &&
 maddy::UnorderedListParser::IsStartingLine(line)
 )
 {
 parser = this->createUnorderedListParser();
 }
 return parser;
 }
 );
 }
}; } 