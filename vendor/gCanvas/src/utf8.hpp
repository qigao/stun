#ifndef GCANVAS_SRC_UTF8_HPP
#define GCANVAS_SRC_UTF8_HPP

#include <cstdint>
#include <stdexcept>
#include <string>

namespace gcanvas::detail
{
    inline std::u32string decode_utf8(const std::string& text)
    {
        std::u32string result;
        result.reserve(text.size());
        const auto malformed = []() -> void {
            throw std::range_error("gCanvas text contains malformed UTF-8");
        };
        const auto continuation = [&](std::size_t index) -> std::uint8_t {
            if (index >= text.size())
                malformed();
            const auto value = static_cast<std::uint8_t>(text[index]);
            if ((value & 0xC0U) != 0x80U)
                malformed();
            return value;
        };

        for (std::size_t index = 0; index < text.size();)
        {
            const auto first = static_cast<std::uint8_t>(text[index]);
            if (first <= 0x7FU)
            {
                result.push_back(static_cast<char32_t>(first));
                ++index;
                continue;
            }
            if (first >= 0xC2U && first <= 0xDFU)
            {
                const std::uint8_t second = continuation(index + 1U);
                result.push_back(static_cast<char32_t>(((first & 0x1FU) << 6U) |
                                                       (second & 0x3FU)));
                index += 2U;
                continue;
            }
            if (first >= 0xE0U && first <= 0xEFU)
            {
                const std::uint8_t second = continuation(index + 1U);
                const std::uint8_t third = continuation(index + 2U);
                if ((first == 0xE0U && second < 0xA0U) ||
                    (first == 0xEDU && second > 0x9FU))
                    malformed();
                result.push_back(static_cast<char32_t>(((first & 0x0FU) << 12U) |
                                                       ((second & 0x3FU) << 6U) |
                                                       (third & 0x3FU)));
                index += 3U;
                continue;
            }
            if (first >= 0xF0U && first <= 0xF4U)
            {
                const std::uint8_t second = continuation(index + 1U);
                const std::uint8_t third = continuation(index + 2U);
                const std::uint8_t fourth = continuation(index + 3U);
                if ((first == 0xF0U && second < 0x90U) ||
                    (first == 0xF4U && second > 0x8FU))
                    malformed();
                result.push_back(static_cast<char32_t>(((first & 0x07U) << 18U) |
                                                       ((second & 0x3FU) << 12U) |
                                                       ((third & 0x3FU) << 6U) |
                                                       (fourth & 0x3FU)));
                index += 4U;
                continue;
            }
            malformed();
        }
        return result;
    }
}

#endif
