#include "Colour.hpp"


static constexpr uint32_t DISTINCT_COLOURS[] =
{
    0xFF000000, 0xFF00FF00, 0xFFFF0000, 0xFF0000FF, 0xFFFEFF01, 0xFFFEA6FF, 0xFF66DBFF, 0xFF016400,
    0xFF670001, 0xFF3A0095, 0xFFB57D00, 0xFFF600FF, 0xFFE8EEFF, 0xFF004D77, 0xFF92FB90, 0xFFFF7600,
    0xFF00FFD5, 0xFF7E93FF, 0xFF6C826A, 0xFF9D02FF, 0xFF0089FE, 0xFF82477A, 0xFFD22D7E, 0xFF00A985,
    0xFF5600FF, 0xFF0024A4, 0xFF7EAE00, 0xFF3B3D68, 0xFFFFC6BD, 0xFF003426, 0xFF93D3BD, 0xFF17B900,
    0xFF8E009E, 0xFF441500, 0xFF9F8CC2, 0xFFA374FF, 0xFFFFD001, 0xFF544700, 0xFFFE6FE5, 0xFF318278,
    0xFFA14C0E, 0xFFCBD091, 0xFF7099BE, 0xFFE88A96, 0xFF0088BB, 0xFF2C0043, 0xFF74FFDE, 0xFFC6FF00,
    0xFF02E5FF, 0xFF000E62, 0xFF9C8F00, 0xFF52FF98, 0xFFB14475, 0xFFFF00B5, 0xFF78FF00, 0xFF416EFF,
    0xFF395F00, 0xFF82686B, 0xFF4EAD5F, 0xFF4057A7, 0xFFD2FFA5, 0xFF67B1FF, 0xFFFF9B00, 0xFFBE5EE8
};

void Colour::set(float red, float green, float blue, float alpha) 
{
    abgr = static_cast<uint8_t>(red * 255.f)         |
            static_cast<uint8_t>(green * 255.f) << 8  |
            static_cast<uint8_t>(blue * 255.f)  << 16 |
            static_cast<uint8_t>(alpha * 255.f) << 24;
}

float Colour::r() const 
{
    return ((abgr & 0xFF) / 255.f);
}

float Colour::g() const 
{
    return (((abgr >> 8) & 0xFF) / 255.f);
}

float Colour::b() const 
{
    return (((abgr >> 16) & 0xFF) / 255.f);
}

float Colour::a() const 
{
    return (((abgr >> 24) & 0xFF) / 255.f);
}

Colour Colour::operator=(const uint32_t colour)
{
    abgr = colour;
    return *this;
}

uint32_t Colour::fromU8(int8_t r, int8_t g, int8_t b, int8_t a) 
{
    return ( r | (g << 8) | (b << 16) | (a << 24));
}

uint32_t Colour::getDistinctColour(uint32_t index) 
{
    return DISTINCT_COLOURS[index % 64];
}
