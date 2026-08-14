#include "../src/utf8.hpp"

#include <array>
#include <stdexcept>
#include <string>

namespace
{
    bool rejects(const std::string& input)
    {
        try
        {
            (void)gcanvas::detail::decode_utf8(input);
            return false;
        }
        catch (const std::range_error&)
        {
            return true;
        }
    }
}

int main()
{
    const std::u32string decoded =
        gcanvas::detail::decode_utf8("A\xC2\xA2\xE2\x82\xAC\xF0\x9F\x98\x80");
    if (decoded != U"A\u00A2\u20AC\U0001F600")
        return 1;

    const std::array<std::string, 7> malformed{{
        "\x80",
        "\xC0\xAF",
        "\xE0\x80\xAF",
        "\xED\xA0\x80",
        "\xF0\x80\x80\xAF",
        "\xF4\x90\x80\x80",
        "\xE2\x82",
    }};
    for (const std::string& input : malformed)
        if (!rejects(input))
            return 2;
    return 0;
}
