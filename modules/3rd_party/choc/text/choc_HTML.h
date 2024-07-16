//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_HTML_HEADER_INCLUDED
#define CHOC_HTML_HEADER_INCLUDED

#include <sstream>
#include <string_view>

namespace choc::html
{

//==============================================================================
/**
    A very minimal helper class for building trees of HTML elements which can
    then be printed as text.
*/
struct HTMLElement
{
    HTMLElement() = default;

    /// Creates an element object with the given tag name
    explicit HTMLElement (std::string elementName);

    /// Appends and returns a given child element to this one.
    HTMLElement& addChild (HTMLElement&& childToAdd);

    /// Creates, adds and returns a reference to a new child element inside this one.
    HTMLElement& addChild (std::string elementName);

    /// Creates, adds and returns a reference to a new child element inside this one,
    /// also calling setInline (true) on the new element.
    HTMLElement& addInlineChild (std::string elementName);

    /// Adds and returns a 'a' element with the given href property.
    HTMLElement& addLink (std::string_view linkURL);
    /// Adds and returns a 'div' element.
    HTMLElement& addDiv();
    /// Adds and returns a 'div' element with a given class name.
    HTMLElement& addDiv (std::string_view className);
    /// Adds and returns a 'p' element.
    HTMLElement& addParagraph();
    /// Adds and returns a 'span' element with the given class property.
    HTMLElement& addSpan (std::string_view classToUse);

    /// Adds a property for this element.
    HTMLElement& setProperty (const std::string& propertyName, std::string_view value);

    /// Sets the 'id' property of this element.
    HTMLElement& setID (std::string_view value);
    /// Sets the 'class' property of this element.
    HTMLElement& setClass (std::string_view value);

    /// Appends a content element to this element's list of children.
    /// Note that this returns the parent object, not the new child, to allow chaining.
    HTMLElement& addContent (std::string_view text);
    /// Adds a raw, unescaped content element as a child of this element.
    HTMLElement& addRawContent (std::string text);
    /// Appends a 'br' element to this element's content.
    HTMLElement& addLineBreak();
    /// Appends an &nbsp; to this element's content.
    HTMLElement& addNBSP (size_t number = 1);

    /// Sets the element to be "inline", which means that it won't add any space or newlines
    /// between its child elements. For things like spans or 'p' elements, you probably want them
    /// to be inline.
    HTMLElement& setInline (bool shouldBeInline);

    /// Returns a text version of this element.
    std::string toDocument (bool includeHeader) const;

    /// Writes this element to some kind of stream object.
    template <typename Output>
    void writeToStream (Output& out, bool includeHeader) const;

    /// Returns the list of child elements
    const std::vector<HTMLElement>& getChildren() const         { return children; }
    /// Returns the list of child elements
    std::vector<HTMLElement>& getChildren()                     { return children; }


private:
    //==============================================================================
    std::string name;
    bool isContent = false, contentIsInline = false;
    std::vector<std::string> properties;
    std::vector<HTMLElement> children;

    struct PrintStatus { bool isAtStartOfLine, isFollowingContent; };

    template <typename Output>
    PrintStatus print (Output&, size_t indent, PrintStatus) const;

    static std::string escapeHTMLString (std::string_view, bool escapeNewLines);
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline HTMLElement::HTMLElement (std::string elementName) : name (std::move (elementName)) {}

inline HTMLElement& HTMLElement::addChild (std::string elementName)
{
    children.emplace_back (std::move (elementName));
    return children.back();
}

inline HTMLElement& HTMLElement::addChild (HTMLElement&& childToAdd)
{
    children.emplace_back (std::move (childToAdd));
    return children.back();
}

inline HTMLElement& HTMLElement::addInlineChild (std::string element)   { return addChild (std::move (element)).setInline (true); }
inline HTMLElement& HTMLElement::addLink (std::string_view linkURL)     { return addChild ("a").setProperty ("href", linkURL); }
inline HTMLElement& HTMLElement::addDiv()                               { return addChild ("div"); }
inline HTMLElement& HTMLElement::addDiv (std::string_view className)    { return addDiv().setClass (className); }
inline HTMLElement& HTMLElement::addParagraph()                         { return addInlineChild ("p"); }
inline HTMLElement& HTMLElement::addSpan (std::string_view classToUse)  { return addInlineChild ("span").setClass (classToUse); }
inline HTMLElement& HTMLElement::setID (std::string_view value)         { return setProperty ("id", value); }
inline HTMLElement& HTMLElement::setClass (std::string_view value)      { return setProperty ("class", value); }
inline HTMLElement& HTMLElement::addContent (std::string_view text)     { return addRawContent (escapeHTMLString (text, false)); }
inline HTMLElement& HTMLElement::addLineBreak()                         { return addRawContent ("<br>"); }
inline HTMLElement& HTMLElement::addNBSP (size_t number)                { std::string s; for (size_t i = 0; i < number; ++i) s += "&nbsp;"; return addRawContent (std::move (s)); }
inline HTMLElement& HTMLElement::setInline (bool shouldBeInline)        { contentIsInline = shouldBeInline; return *this; }

inline HTMLElement& HTMLElement::setProperty (const std::string& propertyName, std::string_view value)
{
    properties.push_back (propertyName + "=\"" + escapeHTMLString (value, true) + '"');
    return *this;
}

inline std::string HTMLElement::toDocument (bool includeHeader) const
{
    std::ostringstream out;
    writeToStream (out, includeHeader);
    return out.str();
}

template <typename Output>
void HTMLElement::writeToStream (Output& out, bool includeHeader) const
{
    if (includeHeader)
        out << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">" << std::endl;

    print (out, 0, { true, false });
}

inline HTMLElement& HTMLElement::addRawContent (std::string text)
{
    auto& child = addChild (std::move (text));
    child.isContent = true;
    return *this;
}

template <typename Output>
HTMLElement::PrintStatus HTMLElement::print (Output& out, size_t indent, PrintStatus status) const
{
    if (! (status.isAtStartOfLine || status.isFollowingContent))
    {
        if (! contentIsInline)
            out << '\n';

        status.isAtStartOfLine = true;
    }

    bool openTagIndented = status.isAtStartOfLine && ! contentIsInline;

    if (openTagIndented)
        out << std::string (indent, ' ');

    status.isAtStartOfLine = false;
    out << '<' << name;

    for (auto& p : properties)
        out << ' ' << p;

    out << '>';

    status.isFollowingContent = false;

    for (auto& c : children)
    {
        if (c.isContent)
        {
            out << c.name;
            status.isFollowingContent = true;
        }
        else
        {
            status = c.print (out, indent + 1, status);
        }
    }

    if (openTagIndented && ! (children.empty() || status.isFollowingContent))
        out << "\n" << std::string (indent, ' ');

    out << "</" << name << ">";

    status.isFollowingContent = false;
    return status;
}

inline std::string HTMLElement::escapeHTMLString (std::string_view text, bool escapeNewLines)
{
    std::string result;
    result.reserve (text.length());

    for (auto character : text)
    {
        auto unicodeChar = static_cast<uint32_t> (character);

        auto isCharLegal = +[] (uint32_t c) -> bool
        {
            return (c >= 'a' && c <= 'z')
                || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9')
                || (c < 127 && std::string_view (" .,;:-()_+=?!$#@[]/|*%~{}\\").find ((char) c) != std::string_view::npos);
        };

        if (isCharLegal (unicodeChar))
        {
            result += character;
        }
        else
        {
            switch (unicodeChar)
            {
                case '<':   result += "&lt;";   break;
                case '>':   result += "&gt;";   break;
                case '&':   result += "&amp;";  break;
                case '"':   result += "&quot;"; break;

                default:
                    if (! escapeNewLines && (unicodeChar == '\n' || unicodeChar == '\r'))
                        result += character;
                    else
                        result += "&#" + std::to_string (unicodeChar) + ';';

                    break;
            }
        }
    }

    return result;
}

} // namespace choc::html

#endif // CHOC_HTML_HEADER_INCLUDED
