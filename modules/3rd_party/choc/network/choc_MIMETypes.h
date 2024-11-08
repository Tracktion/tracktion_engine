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

#ifndef CHOC_MIME_TYPES_HEADER_INCLUDED
#define CHOC_MIME_TYPES_HEADER_INCLUDED

#include "../text/choc_StringUtilities.h"


namespace choc::network
{
    /// Makes a guess at picking a sensible MIME type from a list of common ones,
    /// based on the extension of the filename/path/URL provided. If nothing matches,
    /// it will return `resultIfNotFound`.
    inline std::string getMIMETypeFromFilename (const std::string& filename,
                                                std::string_view resultIfNotFound = "application/text")
    {
        if (auto question = filename.rfind ("?"); question != std::string_view::npos)
            return getMIMETypeFromFilename (filename.substr (0, question));

        auto getExtension = [&] () -> std::string
        {
            if (auto lastDot = filename.rfind ("."); lastDot != std::string_view::npos)
                return choc::text::trim (choc::text::toLowerCase (filename.substr (lastDot + 1)));

            return {};
        };

        if (auto extension = getExtension(); ! extension.empty())
        {
            struct MIMEType { const char* ext; const char* mime; };

            static constexpr const MIMEType types[] =
            {
                { "htm",    "text/html" },
                { "html",   "text/html" },
                { "css",    "text/css" },
                { "txt",    "text/plain" },
                { "md",     "text/markdown" },
                { "js",     "application/javascript" },
                { "wasm",   "application/wasm" },
                { "json",   "application/json" },
                { "xml",    "application/xml" },
                { "wav",    "audio/wav" },
                { "ogg",    "audio/ogg" },
                { "flac",   "audio/flac" },
                { "mid",    "audio/mid" },
                { "mp3",    "audio/mpeg" },
                { "mp4",    "audio/mp4" },
                { "aif",    "audio/x-aiff" },
                { "aiff",   "audio/x-aiff" },
                { "php",    "text/html" },
                { "flv",    "video/x-flv" },
                { "png",    "image/png" },
                { "jpe",    "image/jpeg" },
                { "jpeg",   "image/jpeg" },
                { "jpg",    "image/jpeg" },
                { "gif",    "image/gif" },
                { "bmp",    "image/bmp" },
                { "tiff",   "image/tiff" },
                { "tif",    "image/tiff" },
                { "ico",    "image/vnd.microsoft.icon" },
                { "svg",    "image/svg+xml" },
                { "svgz",   "image/svg+xml" },
                { "otf",    "font/otf" },
                { "sfnt",   "font/sfnt" },
                { "ttf",    "font/ttf" },
                { "woff",   "font/woff" },
                { "woff2",  "font/woff2" }
            };

            for (auto t : types)
                if (extension == t.ext)
                    return t.mime;
        }

        return std::string (resultIfNotFound);
    }
}


#endif // CHOC_MIME_TYPES_HEADER_INCLUDED
