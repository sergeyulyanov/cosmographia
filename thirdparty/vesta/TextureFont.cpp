/*
 * $Revision: 675 $ $Date: 2012-05-22 17:17:55 -0700 (Tue, 22 May 2012) $
 *
 * Copyright by Astos Solutions GmbH, Germany
 *
 * this file is published under the Astos Solutions Free Public License
 * For details on copyright and terms of use see 
 * http://www.astos.de/Astos_Solutions_Free_Public_License.html
 */

#include "TextureFont.h"
#include "DataChunk.h"
#include "Debug.h"
#include "internal/InputDataStream.h"
#include "internal/DefaultFont.h"
#include "OGLHeaders.h"
#include <algorithm>

using namespace vesta;
using namespace Eigen;
using namespace std;


static const unsigned int InvalidGlyphIndex = ~0u;


counted_ptr<TextureFont> TextureFont::ms_defaultFont;


/** Create a new texture font with no glyphs and an undefined 
 *  glyph texture.
 */
TextureFont::TextureFont() :
    m_maxCharacterId(0),
    m_maxAscent(0.0f),
    m_maxDescent(0.0f)
{
}


TextureFont::~TextureFont()
{
}


/** Find the glyph representing the specified character ID.
  *
  * @return A pointer the to glyph record, or NULL if the font doesn't
  * defined a glyph for the character.
  */
const TextureFont::Glyph*
TextureFont::lookupGlyph(wchar_t ch) const
{
    unsigned int charIndex = (unsigned int) ch;
    if (charIndex < m_characterSet.size())
    {
        unsigned int glyphIndex = m_characterSet[charIndex];
        if (glyphIndex != InvalidGlyphIndex)
        {
            return &m_glyphs[glyphIndex];
        }
    }

    return 0;
}


/** Render a string of text at the specified starting poisition. The string encoding
  * is assumed to be Latin-1 (ISO 8859-1).
  *
  * Note that this method will not draw anything on systems with OpenGL ES 2.0 (typically mobile
  * devices.) For compatibility with all systems, use renderStringToBuffer instead.
  */
Vector2f
TextureFont::render(const string& text, const Vector2f& startPosition) const
{
    Vector2f currentPosition = startPosition;

#ifndef VESTA_NO_IMMEDIATE_MODE_3D
    glBegin(GL_QUADS);
    for (unsigned int i = 0; i < text.length(); ++i)
    {
        // The cast to unsigned char is critical for glyph lookup to work correctly;
        // otherwise, extended characters will generate negative indices.
        const Glyph* glyph = lookupGlyph((unsigned char) text[i]);

        if (glyph)
        {
            Vector2f p = currentPosition + glyph->offset;

            glTexCoord2fv(glyph->textureCoords[0].data());
            glVertex2f(p.x(), p.y());
            glTexCoord2fv(glyph->textureCoords[1].data());
            glVertex2f(p.x() + glyph->size.x(), p.y());
            glTexCoord2fv(glyph->textureCoords[2].data());
            glVertex2f(p.x() + glyph->size.x(), p.y() + glyph->size.y());
            glTexCoord2fv(glyph->textureCoords[3].data());
            glVertex2f(p.x(), p.y() + glyph->size.y());

            currentPosition.x() += glyph->advance;
        }
    }
    glEnd();
#endif

    return currentPosition;
}


static inline unsigned int lower6bits(char c)
{
    return (unsigned int) c & 0x3f;
}


/** Render a string of UTF-8 encoded text at the specified starting position.
 *
 *  Note that this method will not draw anything on systems with OpenGL ES 2.0 (typically mobile
 *  devices.) For compatibility with all systems, use renderStringToBuffer instead.
 */
Vector2f
TextureFont::renderUtf8(const string& text, const Vector2f& startPosition) const
{
    Vector2f currentPosition = startPosition;

#ifndef VESTA_NO_IMMEDIATE_MODE_3D
    glBegin(GL_QUADS);
    unsigned int i = 0;

    while (i < text.length())
    {
        unsigned char byte0 = text[i];
        unsigned int decodeBytes = 0;

        if (byte0 < 0x80)
        {
            decodeBytes = 1;
        }
        else if ((byte0 & 0xe0) == 0xc0)
        {
            decodeBytes = 2;
        }
        else if ((byte0 & 0xf0) == 0xe0)
        {
            decodeBytes = 3;
        }
        else if ((byte0 & 0xf8) == 0xf0)
        {
            decodeBytes = 4;
        }
        else if ((byte0 & 0xfc) == 0xf8)
        {
            decodeBytes = 5;
        }
        else if ((byte0 & 0xfe) == 0xfc)
        {
            decodeBytes = 6;
        }

        if (decodeBytes == 0 || i + decodeBytes > text.length())
        {
            // Invalid UTF-8 encoding
            break;
        }

        unsigned int glyphId = 0;
        switch (decodeBytes)
        {
        case 1:
            glyphId = byte0;
            break;
        case 2:
            glyphId = ((byte0 & 0x1f) << 6) |
                      lower6bits(text[i + 1]);
            break;
        case 3:
            glyphId = ((byte0 & 0x0f) << 12) |
                      (lower6bits(text[i + 1]) << 6) |
                      lower6bits(text[i + 2]);
            break;
        case 4:
            glyphId = ((byte0 & 0x07) << 18) |
                      (lower6bits(text[i + 1]) << 12) |
                      (lower6bits(text[i + 2]) << 6)  |
                      lower6bits(text[i + 3]);
            break;
        case 5:
            glyphId = ((byte0 & 0x03) << 24) |
                      (lower6bits(text[i + 1]) << 18) |
                      (lower6bits(text[i + 2]) << 12) |
                      (lower6bits(text[i + 3]) << 6)  |
                      lower6bits(text[i + 4]);
            break;
        case 6:
            glyphId = ((byte0 & 0x01) << 30)    |
                      (lower6bits(text[i + 1]) << 24) |
                      (lower6bits(text[i + 2]) << 18) |
                      (lower6bits(text[i + 3]) << 12) |
                      (lower6bits(text[i + 4]) << 6)  |
                      lower6bits(text[i + 5]);
            break;
        default:
            break;
        }

        const Glyph* glyph = lookupGlyph(glyphId);

        if (glyph)
        {
            Vector2f p = currentPosition + glyph->offset;

            glTexCoord2fv(glyph->textureCoords[0].data());
            glVertex2f(p.x(), p.y());
            glTexCoord2fv(glyph->textureCoords[1].data());
            glVertex2f(p.x() + glyph->size.x(), p.y());
            glTexCoord2fv(glyph->textureCoords[2].data());
            glVertex2f(p.x() + glyph->size.x(), p.y() + glyph->size.y());
            glTexCoord2fv(glyph->textureCoords[3].data());
            glVertex2f(p.x(), p.y() + glyph->size.y());

            currentPosition.x() += glyph->advance;
        }

        i += decodeBytes;
    }
    glEnd();
#endif
    
    return currentPosition;
}


/** Render a string at the specified starting position. The string encoding is specified as a .
  * parameter. The position at the end of the string is returned in order to make it possible
  * to place strings next to each other.
  *
  * Note that this method will not draw anything on systems with OpenGL ES 2.0 (typically mobile
  * devices.) For compatibility with all systems, use renderStringToBuffer instead.
  *
  * \return position immediately after final character in string
  */
Vector2f
TextureFont::renderEncodedString(const string& text,
                                 const Eigen::Vector2f& startPosition,
                                 Encoding encoding) const
{
    switch (encoding)
    {
    // ASCII and Latin-1 are currently treated as identical encodings
    case Ascii:
    case Latin1:
        return render(text, startPosition);
    case Utf8:
        return renderUtf8(text, startPosition);
    default:
        VESTA_WARNING("Unknown string encoding.");
        return startPosition;
    }
}


/** Fill a buffer with vertices for a string of glyphs from this
 *  font. If there is not enough room in the buffer, not all of
 *  the characters in the string will be output.
 *
 * \return position immediately after final character in string
 */
Vector2f
TextureFont::renderStringToBuffer(const std::string& text,
                                  const Eigen::Vector2f &startPosition,
                                  Encoding encoding,
                                  char* vertexData,
                                  unsigned int vertexDataSize,
                                  unsigned int* verticesUsed) const
{
    if (vertexData == NULL)
    {
        return startPosition;
    }
    
    switch (encoding)
    {
    // ASCII and Latin-1 are currently treated as identical encodings
    case Ascii:
    case Latin1:
        return renderLatin1ToBuffer(text, startPosition, vertexData, vertexDataSize, verticesUsed);
    case Utf8:
        return renderUtf8ToBuffer(text, startPosition, vertexData, vertexDataSize, verticesUsed);
    default:
        VESTA_WARNING("Unknown string encoding.");
        return startPosition;
    }
}


static inline void
OutputPositionTexVertexToBuffer(float x,
                                float y,
                                const Vector2f& texCoord,
                                float* buffer)
{
    buffer[0] = x;
    buffer[1] = y;
    buffer[2] = 0.0f;
    buffer[3] = texCoord.x();
    buffer[4] = texCoord.y();
}


static const unsigned int VERTEX_SIZE_FLOATS = 5;
static const unsigned int VERTEX_SIZE = VERTEX_SIZE_FLOATS * sizeof(float);
static const unsigned int VERTEX_COUNT_PER_GLYPH = 6;
static const unsigned int GLYPH_SIZE = VERTEX_SIZE * VERTEX_COUNT_PER_GLYPH;
static const unsigned int GLYPH_SIZE_FLOATS = GLYPH_SIZE / sizeof(float);

static void
OutputGlyphToBuffer(const TextureFont::Glyph& glyph,
                    const Vector2f& position,
                    float* buffer)
{
    float left = position.x();
    float bottom = position.y();
    float right = position.x() + glyph.size.x();
    float top = position.y() + glyph.size.y();

    OutputPositionTexVertexToBuffer(left,  bottom, glyph.textureCoords[0], buffer);
    OutputPositionTexVertexToBuffer(right, bottom, glyph.textureCoords[1], buffer + VERTEX_SIZE_FLOATS * 1);
    OutputPositionTexVertexToBuffer(right, top,    glyph.textureCoords[2], buffer + VERTEX_SIZE_FLOATS * 2);

    OutputPositionTexVertexToBuffer(left,  bottom, glyph.textureCoords[0], buffer + VERTEX_SIZE_FLOATS * 3);
    OutputPositionTexVertexToBuffer(right, top,    glyph.textureCoords[2], buffer + VERTEX_SIZE_FLOATS * 4);
    OutputPositionTexVertexToBuffer(left,  top,    glyph.textureCoords[3], buffer + VERTEX_SIZE_FLOATS * 5);
}


// Private method that performs the actual work of drawing glyphs
Vector2f
TextureFont::renderUtf8ToBuffer(const std::string& text,
                                const Eigen::Vector2f& startPosition,
                                char* vertexData,
                                unsigned int vertexDataSize,
                                unsigned int* vertexCount) const
{
    Vector2f currentPosition = startPosition;
    unsigned int glyphCount = 0;
    unsigned int maxGlyphs = 0x10000000;

#ifndef VESTA_NO_IMMEDIATE_MODE_3D
    if (vertexData)
    {
        maxGlyphs = vertexDataSize / GLYPH_SIZE;
    }
    else
    {
        glBegin(GL_QUADS);
    }
#endif
    unsigned int i = 0;

    while (i < text.length() && glyphCount < maxGlyphs)
    {
        unsigned char byte0 = text[i];
        unsigned int decodeBytes = 0;

        if (byte0 < 0x80)
        {
            decodeBytes = 1;
        }
        else if ((byte0 & 0xe0) == 0xc0)
        {
            decodeBytes = 2;
        }
        else if ((byte0 & 0xf0) == 0xe0)
        {
            decodeBytes = 3;
        }
        else if ((byte0 & 0xf8) == 0xf0)
        {
            decodeBytes = 4;
        }
        else if ((byte0 & 0xfc) == 0xf8)
        {
            decodeBytes = 5;
        }
        else if ((byte0 & 0xfe) == 0xfc)
        {
            decodeBytes = 6;
        }

        if (decodeBytes == 0 || i + decodeBytes > text.length())
        {
            // Invalid UTF-8 encoding
            break;
        }

        unsigned int glyphId = 0;
        switch (decodeBytes)
        {
        case 1:
            glyphId = byte0;
            break;
        case 2:
            glyphId = ((byte0 & 0x1f) << 6) |
                      lower6bits(text[i + 1]);
            break;
        case 3:
            glyphId = ((byte0 & 0x0f) << 12) |
                      (lower6bits(text[i + 1]) << 6) |
                      lower6bits(text[i + 2]);
            break;
        case 4:
            glyphId = ((byte0 & 0x07) << 18) |
                      (lower6bits(text[i + 1]) << 12) |
                      (lower6bits(text[i + 2]) << 6)  |
                      lower6bits(text[i + 3]);
            break;
        case 5:
            glyphId = ((byte0 & 0x03) << 24) |
                      (lower6bits(text[i + 1]) << 18) |
                      (lower6bits(text[i + 2]) << 12) |
                      (lower6bits(text[i + 3]) << 6)  |
                      lower6bits(text[i + 4]);
            break;
        case 6:
            glyphId = ((byte0 & 0x01) << 30)    |
                      (lower6bits(text[i + 1]) << 24) |
                      (lower6bits(text[i + 2]) << 18) |
                      (lower6bits(text[i + 3]) << 12) |
                      (lower6bits(text[i + 4]) << 6)  |
                      lower6bits(text[i + 5]);
            break;
        default:
            break;
        }

        const Glyph* glyph = lookupGlyph(glyphId);

        if (glyph)
        {
            Vector2f p = currentPosition + glyph->offset;

            if (vertexData)
            {
                OutputGlyphToBuffer(*glyph, p, reinterpret_cast<float*>(vertexData) + GLYPH_SIZE_FLOATS * glyphCount);
            }
            else
            {
#ifndef VESTA_NO_IMMEDIATE_MODE_3D
                glTexCoord2fv(glyph->textureCoords[0].data());
                glVertex2f(p.x(), p.y());
                glTexCoord2fv(glyph->textureCoords[1].data());
                glVertex2f(p.x() + glyph->size.x(), p.y());
                glTexCoord2fv(glyph->textureCoords[2].data());
                glVertex2f(p.x() + glyph->size.x(), p.y() + glyph->size.y());
                glTexCoord2fv(glyph->textureCoords[3].data());
                glVertex2f(p.x(), p.y() + glyph->size.y());
#endif
            }

            currentPosition.x() += glyph->advance;
            ++glyphCount;
        }

        i += decodeBytes;
    }

#ifndef VESTA_NO_IMMEDIATE_MODE_3D
    if (!vertexData)
    {
        glEnd();
    }
#endif

    if (vertexCount)
    {
        *vertexCount = VERTEX_COUNT_PER_GLYPH * glyphCount;
    }

    return currentPosition;
}


// Private method that performs the actual work of drawing glyphs
Vector2f
TextureFont::renderLatin1ToBuffer(const std::string& text,
                                  const Eigen::Vector2f& startPosition,
                                  char* vertexData,
                                  unsigned int vertexDataSize,
                                  unsigned int* vertexCount) const
{
    Vector2f currentPosition = startPosition;
    unsigned int glyphCount = 0;
    unsigned int maxGlyphs = 0x10000000;
    
#ifndef VESTA_NO_IMMEDIATE_MODE_3D
    if (vertexData)
    {
        maxGlyphs = vertexDataSize / GLYPH_SIZE;
    }
    else
    {
        glBegin(GL_QUADS);
    }
#endif
    unsigned int i = 0;
    
    while (i < text.length() && glyphCount < maxGlyphs)
    {
        // The cast to unsigned char is critical for glyph lookup to work correctly;
        // otherwise, extended characters will generate negative indices.
        const Glyph* glyph = lookupGlyph((unsigned char) text[i]);
        
        if (glyph)
        {
            Vector2f p = currentPosition + glyph->offset;
            
            if (vertexData)
            {
                OutputGlyphToBuffer(*glyph, p, reinterpret_cast<float*>(vertexData) + GLYPH_SIZE_FLOATS * glyphCount);
            }
            else
            {
#ifndef VESTA_NO_IMMEDIATE_MODE_3D
                glTexCoord2fv(glyph->textureCoords[0].data());
                glVertex2f(p.x(), p.y());
                glTexCoord2fv(glyph->textureCoords[1].data());
                glVertex2f(p.x() + glyph->size.x(), p.y());
                glTexCoord2fv(glyph->textureCoords[2].data());
                glVertex2f(p.x() + glyph->size.x(), p.y() + glyph->size.y());
                glTexCoord2fv(glyph->textureCoords[3].data());
                glVertex2f(p.x(), p.y() + glyph->size.y());
#endif
            }
            
            currentPosition.x() += glyph->advance;
            ++glyphCount;
        }
        
        i++;
    }
    
#ifndef VESTA_NO_IMMEDIATE_MODE_3D
    if (!vertexData)
    {
        glEnd();
    }
#endif
    
    if (vertexCount)
    {
        *vertexCount = VERTEX_COUNT_PER_GLYPH * glyphCount;
    }
    
    return currentPosition;
}


/** Compute the width of a string of text in pixels.
 */
float
TextureFont::textWidth(const string& text) const
{
    float width = 0.0f;

    for (unsigned int i = 0; i < text.length(); ++i)
    {
        const Glyph* glyph = lookupGlyph((unsigned char) text[i]);
        if (glyph)
        {
            width += glyph->advance;
        }
    }

    return width;
}


/** Get the maximum height above the baseline of any glyph in the font.
  * The returned value is in units of pixels.
  */
float
TextureFont::maxAscent() const
{
    return m_maxAscent;
}


/** Get the maximum distance that any glyph extends below the baseline.
  * The returned value is in units of pixels.
  */
float
TextureFont::maxDescent() const
{
    return m_maxDescent;
}


/** Bind the font texture. */
void
TextureFont::bind() const
{
    if (!m_glyphTexture.isNull())
    {
        glBindTexture(GL_TEXTURE_2D, m_glyphTexture->id());
    }
}


/** Generate an OpenGL texture with all the glyph bitmaps for this font.
 *
 * @param width width of the font texture
 * @param height height of the font texture
 * @param pixels an array of pixels with dimensions width * height. Each
 *        pixel is 8-bit value with 0 = transparent, 255 = opaque, and other
 *        values indicating intermediate opacities
 * @return true if the font texture was created successfully
 */
bool
TextureFont::buildFontTexture(unsigned int width,
                              unsigned int height,
                              unsigned char* pixels)
{
    GLuint texId = 0;

    glGenTextures(1, &texId);
    if (texId == 0)
    {
        return false;
    }

    // Delete the old texture (if any)

    glBindTexture(GL_TEXTURE_2D, texId);

    // Disable filtering to prevent blurriness
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_ALPHA,
                 width, height,
                 0,
                 GL_ALPHA,
                 GL_UNSIGNED_BYTE,
                 pixels);

    TextureProperties glyphTextureProperties(TextureProperties::Clamp);
    glyphTextureProperties.usage = TextureProperties::AlphaTexture;

    m_glyphTexture = new TextureMap(texId, glyphTextureProperties);

    return true;
}


/** Add a new glyph to the font.
 */
void
TextureFont::addGlyph(const Glyph& glyph)
{
    m_glyphs.push_back(glyph);
    m_maxCharacterId = max(m_maxCharacterId, glyph.characterId);
    m_maxAscent = max(m_maxAscent, glyph.size.y() + glyph.offset.y());
    m_maxDescent = max(m_maxDescent, -glyph.offset.y());
}


/** Build the table that maps character ids to glyphs.
 */
void
TextureFont::buildCharacterSet()
{
    // Initialize an empty character table of adequate size
    m_characterSet.resize((unsigned int) m_maxCharacterId + 1);
    for (unsigned int i = 0; i <= m_maxCharacterId; ++i)
    {
        m_characterSet[i] = InvalidGlyphIndex;
    }

    for (vector<Glyph>::const_iterator iter = m_glyphs.begin();
         iter != m_glyphs.end(); iter++)
    {
        if (iter->characterId < m_characterSet.size())
        {
            m_characterSet[iter->characterId] = (unsigned int) (iter - m_glyphs.begin());
        }
    }
}


/** Load a texture font from a chunk of data containing font data
  * in the TXF format used by GLUT.
  *
  * \returns true if the data is a valid font and the font texture
  * could be created.
  */
bool
TextureFont::loadTxf(const DataChunk* data)
{
    string str(data->data(), data->size());
    InputDataStream in(str);
    in.setByteOrder(InputDataStream::BigEndian);

    char header[4];
    in.readData(header, sizeof(header));
    if (in.status() != InputDataStream::Good)
    {
        VESTA_LOG("Incomplete header in texture font.");
        return NULL;
    }

    if (string(header, sizeof(header)) != "\377txf")
    {
        VESTA_LOG("Bad header in texture font file.");
        return NULL;
    }

    v_uint32 endianness = in.readUint32();
    if (endianness == 0x12345678)
    {
        in.setByteOrder(InputDataStream::BigEndian);
    }
    else if (endianness == 0x78563412)
    {
        in.setByteOrder(InputDataStream::LittleEndian);
    }
    else
    {
        VESTA_LOG("Bad endianness in texture font header.");
        return NULL;
    }

    v_uint32 format = in.readUint32();
    v_uint32 glyphTextureWidth = in.readUint32();
    v_uint32 glyphTextureHeight = in.readUint32();
    /* v_uint32 maxAscent = */ in.readUint32();
    /* v_uint32 maxDescent = */ in.readUint32();
    v_uint32 glyphCount = in.readUint32();

    if (in.status() != InputDataStream::Good)
    {
        VESTA_LOG("Error reading texture font header values.");
        return NULL;
    }

    if (format != 0)
    {
        VESTA_LOG("Texture font has wrong type (bitmap fonts not supported.)");
        return NULL;
    }

    if (glyphTextureWidth == 0 || glyphTextureWidth > 4096 ||
        glyphTextureHeight == 0 || glyphTextureHeight > 4096)
    {
        VESTA_LOG("Bad glyph texture size in font (%dx%d)", glyphTextureWidth, glyphTextureHeight);
        return NULL;
    }

    Vector2f texelScale(1.0f / glyphTextureWidth, 1.0f / glyphTextureHeight);
    Vector2f halfTexel = texelScale * 0.5f;

    for (v_uint32 i = 0; i < glyphCount; i++)
    {
        v_uint16 characterId = in.readUint16();
        v_uint8 glyphWidth = in.readUbyte();
        v_uint8 glyphHeight = in.readUbyte();
        v_int8 xoffset = in.readByte();
        v_int8 yoffset = in.readByte();
        v_int8 advance = in.readByte();
        /* v_int8 unused = */ in.readByte();
        v_uint16 x = in.readUint16();
        v_uint16 y = in.readUint16();

        if (in.status() != InputDataStream::Good)
        {
            VESTA_LOG("Error reading glyph %d in texture font.", i + 1);
            return false;
        }

        Vector2f normalizedSize = texelScale.cwiseProduct(Vector2f(glyphWidth, glyphHeight));
        Vector2f normalizedPosition = texelScale.cwiseProduct(Vector2f(x, y)) + halfTexel;

        TextureFont::Glyph glyph;
        glyph.characterId = characterId;
        glyph.advance = advance;
        glyph.offset = Vector2f(xoffset, yoffset);
        glyph.size = Vector2f(glyphWidth, glyphHeight);
        glyph.textureCoords[0] = normalizedPosition;
        glyph.textureCoords[1] = normalizedPosition + Vector2f(normalizedSize.x(), 0.0f);
        glyph.textureCoords[2] = normalizedPosition + normalizedSize;
        glyph.textureCoords[3] = normalizedPosition + Vector2f(0.0f, normalizedSize.y());

        addGlyph(glyph);
    }

    unsigned int pixelCount = glyphTextureWidth * glyphTextureHeight;
    unsigned char* pixels = new unsigned char[pixelCount];
    in.readData(reinterpret_cast<char*>(pixels), pixelCount);
    if (in.status() != InputDataStream::Good)
    {
        VESTA_LOG("Error reading pixel data in texture font.");
        delete[] pixels;
        return false;
    }

    buildCharacterSet();
    buildFontTexture(glyphTextureWidth, glyphTextureHeight, pixels);

    delete[] pixels;

    return true;
}


/** Load a texture font from a chunk of data containing font data
  * in the TXF format used by GLUT.
  */
TextureFont*
TextureFont::LoadTxf(const DataChunk* data)
{
    TextureFont* font = new TextureFont();
    if (font->loadTxf(data))
    {
        // TODO: Should skip the addRef
        font->addRef();
    }
    else
    {
        delete font;
        font = NULL;
    }

    return font;
}


/** Get the default font. This will always be available provided that
  * an OpenGL has been initialized (or more precisely, that there is a current
  * and valid OpenGL context.)
  */
TextureFont*
TextureFont::GetDefaultFont()
{
    if (ms_defaultFont.isNull())
    {
        VESTA_LOG("Creating default font...");
        DataChunk* data = GetDefaultFontData();
        if (data == NULL)
        {
            VESTA_WARNING("Internal error occurred when creating default font.");
        }
        else
        {
            TextureFont* font = LoadTxf(data);
            if (!font)
            {
                VESTA_WARNING("Failed to create default font. Font data is not valid.");
            }
            ms_defaultFont = font;
        }
    }

    return ms_defaultFont.ptr();
}
