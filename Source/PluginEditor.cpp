#include "PluginEditor.h"
#include <functional>

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "dev"
#endif

//==============================================================================
namespace pal
{
const juce::Colour ironDark  { 0xff1d1815 };
const juce::Colour iron      { 0xff3a322c };
const juce::Colour rustDeep  { 0xff4a1d0c };
const juce::Colour rust      { 0xff9a4518 };
const juce::Colour rustLight { 0xffc0702a };
const juce::Colour paint     { 0xffd6c7a6 };   // dirty cream stencil paint
const juce::Colour paintDim  { 0xffa8997c };
const juce::Colour signal    { 0xffd2452a };   // rusty red for active things
const juce::Colour signalHot { 0xffff6a3d };
const juce::Colour hazard    { 0xffc99a22 };   // worn safety yellow
const juce::Colour screen    { 0xff0c0a08 };
}

//==============================================================================
namespace tex
{
inline float hash (int x, int y, int seed)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float) (h & 0xffffffu) / 16777215.0f;
}
inline float smooth (float t) { return t * t * (3.0f - 2.0f * t); }
inline float smoothstep (float a, float b, float x) { return smooth (juce::jlimit (0.0f, 1.0f, (x - a) / (b - a))); }
inline float vnoise (float x, float y, int seed)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    const float fx = x - (float) xi, fy = y - (float) yi;
    const float a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed), c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
    const float u = smooth (fx), v = smooth (fy);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline float fbm (float x, float y, int seed, int octaves)
{
    float s = 0, amp = 0.5f, f = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) { s += amp * vnoise (x * f, y * f, seed + i * 31); norm += amp; f *= 2.03f; amp *= 0.5f; }
    return s / norm;
}
struct RGB { float r, g, b; };
inline RGB rgb (juce::Colour c) { return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() }; }
inline RGB mix (RGB a, RGB b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }
inline RGB mul (RGB a, float k) { return { a.r * k, a.g * k, a.b * k }; }

inline RGB rustAt (float x, float y, float rustBias, int seed)
{
    const float n      = fbm (x / 170.0f, y / 170.0f, seed, 4);
    const float detail = fbm (x / 36.0f, y / 36.0f, seed + 7, 3);
    const float fine   = vnoise (x / 7.0f, y / 7.0f, seed + 13);
    const float mask   = smoothstep (0.45f, 0.62f, n + (detail - 0.5f) * 0.35f + rustBias);
    RGB base = mix (rgb (pal::ironDark), rgb (pal::iron), detail);
    RGB r    = mix (rgb (pal::rustDeep), rgb (pal::rust), fbm (x / 20.0f, y / 20.0f, seed + 3, 2));
    if (fine > 0.72f) r = mix (r, rgb (pal::rustLight), (fine - 0.72f) * 2.5f);
    return mix (base, r, mask);
}

// Paint-chip a rendered image: knock out chunks of alpha with noise, plus grain.
inline void distress (juce::Image& img, float scale, float wear, int seed)
{
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bd.height; ++y)
        for (int x = 0; x < bd.width; ++x)
        {
            auto c = bd.getPixelColour (x, y);
            if (c.getAlpha() == 0) continue;
            const float n = fbm ((float) x / (7.0f * scale), (float) y / (7.0f * scale), seed, 3);
            const float chip = n < wear ? 0.08f : 1.0f;
            const float grain = 0.7f + 0.3f * hash (x, y, seed + 99);
            bd.setPixelColour (x, y, c.withMultipliedAlpha (chip * grain));
        }
}

inline juce::Font stencil (float size) { return juce::Font (juce::FontOptions ("Impact", size, juce::Font::plain)); }
inline juce::Font plainBold (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

inline juce::Image distressedText (const juce::String& text, int w, int h, juce::Font font, juce::Colour col,
                                   juce::Justification just, float scale, float wear, int seed, float kerning = 0.0f)
{
    juce::Image img (juce::Image::ARGB, juce::roundToInt ((float) w * scale), juce::roundToInt ((float) h * scale), true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.setColour (col);
        g.setFont (font.withExtraKerningFactor (kerning));
        g.drawFittedText (text, 0, 0, w, h, just, 1, 0.55f);   // squeeze rather than clip if the font runs wide
    }
    distress (img, scale, wear, seed);
    return img;
}
} // namespace tex

//==============================================================================
// The Castle Danger crest: a castle silhouette under a blood moon, on a riveted iron medallion.
static void drawCastleCrest (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto c = r.getCentre();
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;

    // iron ring
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (r.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff1a1410), c.x + R, c.y + R, true));
    g.fillEllipse (r);
    g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (r.reduced (1.0f), 1.6f);
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i / 8.0f * juce::MathConstants<float>::twoPi + 0.39f;
        const float rx = c.x + std::cos (a) * R * 0.88f, ry = c.y + std::sin (a) * R * 0.88f;
        g.setColour (juce::Colour (0xff8a7a68)); g.fillEllipse (rx - 1.8f, ry - 1.8f, 3.6f, 3.6f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (rx - 1.8f, ry - 1.8f, 3.6f, 3.6f, 0.6f);
    }

    // night sky + blood moon
    const auto inner = r.reduced (R * 0.2f);
    const float ir = inner.getWidth() * 0.5f;
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addEllipse (inner);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0c08), c.x, inner.getY(), juce::Colour (0xff080505), c.x, inner.getBottom(), false));
    g.fillEllipse (inner);
    const juce::Rectangle<float> moon (c.x - ir * 0.55f, inner.getY() + ir * 0.12f, ir * 1.1f, ir * 1.1f);
    juce::DropShadow (pal::signalHot.withAlpha (0.55f), (int) (ir * 0.45f), {}).drawForPath (g, [&] { juce::Path p; p.addEllipse (moon); return p; }());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff7a4a), moon.getX() + moon.getWidth() * 0.35f, moon.getY() + moon.getHeight() * 0.3f,
                                             juce::Colour (0xff8a1408), moon.getRight(), moon.getBottom(), true));
    g.fillEllipse (moon);
    g.setColour (juce::Colour (0x40400000));
    g.fillEllipse (moon.getX() + moon.getWidth() * 0.55f, moon.getY() + moon.getHeight() * 0.25f, moon.getWidth() * 0.16f, moon.getWidth() * 0.16f);
    g.fillEllipse (moon.getX() + moon.getWidth() * 0.25f, moon.getY() + moon.getHeight() * 0.55f, moon.getWidth() * 0.12f, moon.getWidth() * 0.12f);

    // castle silhouette
    const float x0 = inner.getX(), y0 = inner.getY(), w = inner.getWidth(), h = inner.getHeight();
    auto X = [&] (float f) { return x0 + f * w; };
    auto Y = [&] (float f) { return y0 + f * h; };
    juce::Path castle;
    auto tower = [&] (float l, float rgt, float top, int merlons)
    {
        castle.addRectangle (X (l), Y (top), X (rgt) - X (l), Y (1.05f) - Y (top));
        const float mw = (rgt - l) / (float) (merlons * 2 - 1);
        for (int i = 0; i < merlons; ++i)
            castle.addRectangle (X (l + (float) i * 2.0f * mw), Y (top - 0.06f), X (l + mw) - X (l), Y (0.06f) - Y (0.0f) + 0.5f);
    };
    tower (0.08f, 0.30f, 0.50f, 3);           // left tower
    tower (0.70f, 0.92f, 0.50f, 3);           // right tower
    tower (0.38f, 0.62f, 0.36f, 3);           // keep
    tower (0.30f, 0.70f, 0.64f, 5);           // curtain wall
    castle.addTriangle (X (0.08f), Y (0.44f), X (0.19f), Y (0.27f), X (0.30f), Y (0.44f));   // spires
    castle.addTriangle (X (0.70f), Y (0.44f), X (0.81f), Y (0.27f), X (0.92f), Y (0.44f));
    castle.setUsingNonZeroWinding (true);
    g.setColour (juce::Colour (0xff070505)); g.fillPath (castle);
    // gate + lit windows
    juce::Path gate;
    gate.addRectangle (X (0.44f), Y (0.84f), X (0.56f) - X (0.44f), Y (0.2f) - Y (0.0f));
    gate.addEllipse (X (0.44f), Y (0.79f), X (0.56f) - X (0.44f), Y (0.1f) - Y (0.0f));
    g.setColour (juce::Colour (0xff2a0c06)); g.fillPath (gate);
    g.setColour (pal::signalHot.withAlpha (0.9f));
    for (auto p : { juce::Point<float> (0.19f, 0.62f), { 0.81f, 0.62f }, { 0.5f, 0.5f } })
        g.fillRect (X (p.x) - w * 0.018f, Y (p.y), w * 0.036f, h * 0.07f);
}

//==============================================================================
FreakLookAndFeel::FreakLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, pal::paintDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::Label::textColourId, pal::paintDim);
    setColour (juce::ComboBox::textColourId, pal::paint);
    setColour (juce::ComboBox::arrowColourId, pal::signal);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1411));
    setColour (juce::PopupMenu::textColourId, pal::paint);
    setColour (juce::PopupMenu::headerTextColourId, pal::rustLight);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::rust);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, pal::paint);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f0b09));
    setColour (juce::TextEditor::textColourId, pal::paint);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff5a3420));
    setColour (juce::TextEditor::focusedOutlineColourId, pal::rustLight);
    setColour (juce::TextEditor::highlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, pal::signalHot);

    rustDisc = juce::Image (juce::Image::ARGB, 128, 128, true);
    juce::Image::BitmapData bd (rustDisc, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
        {
            const float n = tex::fbm ((float) x / 22.0f, (float) y / 22.0f, 501, 4);
            const float a = tex::smoothstep (0.5f, 0.7f, n);
            const auto c = tex::rustAt ((float) x * 2.0f, (float) y * 2.0f, 0.3f, 777);
            bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.r, c.g, c.b, a * 0.85f));
        }
}

juce::Font FreakLookAndFeel::getComboBoxFont (juce::ComboBox&) { return tex::plainBold (13.0f); }
juce::Font FreakLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return tex::stencil (juce::jmin (15.0f, (float) h * 0.62f)).withExtraKerningFactor (0.08f); }

void FreakLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (down ? pal::rustDeep : juce::Colour (0xff3a2a20), 0, r.getY(),
                                             down ? juce::Colour (0xff2a120a) : juce::Colour (0xff17110e), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hi ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
}

void FreakLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff0f0b09)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.isMouseOver (true) ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
    juce::Path arrow; const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (pal::signal); g.fillPath (arrow);
}

void FreakLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = juce::jmax (2.5f, r * 0.11f);
    const float alpha = s.isEnabled() ? 1.0f : 0.3f;

    // tick marks stamped round the dial
    g.setColour (pal::paintDim.withAlpha (0.45f * alpha));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle) - juce::MathConstants<float>::halfPi;
        const float r1 = r + 1.0f, r0 = r - (i % 5 == 0 ? 5.0f : 3.0f);
        g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0, c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, i % 5 == 0 ? 1.4f : 0.9f);
    }

    const float ar = r - thick - 4.0f;
    juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff120e0c).withAlpha (alpha)); g.strokePath (track, juce::PathStrokeType (thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff3a2418).withAlpha (alpha)); g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // bipolar knobs (GAIN, DEPTH) sweep out from zero
    float from = startAngle;
    if (s.getProperties().contains ("bip")) from = startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle);
    juce::Path val; val.addCentredArc (c.x, c.y, ar, ar, 0, std::min (from, angle), std::max (from, angle), true);
    const auto base = s.getProperties().contains ("col") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["col"]) : pal::signal;
    const auto arcCol = (s.isMouseOverOrDragging() ? base.brighter (0.3f) : base).withAlpha (alpha);
    g.setColour (arcCol.withAlpha (0.25f * alpha));
    g.strokePath (val, juce::PathStrokeType (thick + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arcCol);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float br = ar - thick * 1.4f;
    if (br < 4.0f) return;
    const juce::Rectangle<float> body (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (body.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c).withAlpha (alpha), c.x - br * 0.5f, c.y - br * 0.7f,
                                             juce::Colour (0xff17120f), c.x + br * 0.6f, c.y + br, true));
    g.fillEllipse (body);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (body);
        g.reduceClipRegion (clip);
        const float off = (float) (((juce::uint64) (juce::pointer_sized_int) &s >> 4) % 64);
        g.setOpacity (0.9f * alpha);
        g.drawImage (rustDisc, body.expanded (br * 0.4f).translated (-off * 0.3f, off * 0.2f), juce::RectanglePlacement::stretchToFit);
    }
    g.setColour (juce::Colour (0xff0f0b09).withAlpha (0.8f));
    for (int i = 0; i < 32; ++i)
    {
        const float a = (float) i / 32.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::cos (a) * br * 0.87f, c.y + std::sin (a) * br * 0.87f,
                    c.x + std::cos (a) * br, c.y + std::sin (a) * br, 1.0f);
    }
    g.setColour (juce::Colour (0xff0b0807)); g.drawEllipse (body, 1.2f);

    juce::Path p; p.addRoundedRectangle (-1.8f, -br * 0.92f, 3.6f, br * 0.6f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x + 0.8f, c.y + 1.0f));
    g.setColour (pal::paint.withAlpha (alpha));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

// toggles: an indicator lamp + stencilled label on a rusty plate
void FreakLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool warn = b.getName() == "listen";
    const auto lit = warn ? pal::hazard : pal::signalHot;
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff3a1a10) : juce::Colour (0xff2a221d), 0, r.getY(),
                                             juce::Colour (0xff120d0b), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? lit.withAlpha (0.8f) : (hi ? pal::rustLight : juce::Colour (0xff5a3420))); g.drawRoundedRectangle (r, 3.0f, 1.2f);

    const float d = r.getHeight() * 0.42f;
    auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
    if (on)
    {
        juce::Path lp; lp.addEllipse (led);
        juce::DropShadow (lit.withAlpha (0.9f), 10, {}).drawForPath (g, lp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0c0), led.getCentreX(), led.getCentreY(), lit, led.getX(), led.getY(), true));
    }
    else g.setColour (juce::Colour (0xff2a1c16));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (led, 1.0f);

    g.setColour (on ? pal::paint : pal::paintDim);
    g.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.1f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (r.getHeight() * 1.0f).withTrimmedRight (6), juce::Justification::centredLeft);
}


//==============================================================================
// The Freak: band colours
namespace bandpal
{
inline juce::Colour col (int b)
{
    static const juce::Colour c[freak::kBands] { juce::Colour (0xffff6a3d), juce::Colour (0xffe0b030), juce::Colour (0xffd6c7a6),
                                                 juce::Colour (0xff6fbfa0), juce::Colour (0xffc080d8), juce::Colour (0xff62a8e0) };
    return c[juce::jlimit (0, freak::kBands - 1, b)];
}
inline juce::String typeName (int t) { return t == freak::lowShelf ? "LOW SHELF" : (t == freak::highShelf ? "HIGH SHELF" : "BELL"); }
inline juce::String freqText (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + "k" : juce::String (juce::roundToInt (f)); }
}

//==============================================================================
Selector::Selector (juce::StringArray l) : labels (std::move (l)) {}

void Selector::bind (juce::RangedAudioParameter& p)
{
    att = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { index = juce::roundToInt (v); repaint(); if (onChange) onChange(); });
    att->sendInitialUpdate();
}

int Selector::segmentAt (juce::Point<int> p) const
{
    if (labels.isEmpty()) return -1;
    return juce::jlimit (0, labels.size() - 1, p.x * labels.size() / juce::jmax (1, getWidth()));
}

void Selector::mouseDown (const juce::MouseEvent& e) { if (att) att->setValueAsCompleteGesture ((float) segmentAt (e.getPosition())); }
void Selector::mouseMove (const juce::MouseEvent& e) { const int h = segmentAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }

void Selector::paint (juce::Graphics& g)
{
    const int n = labels.size();
    const float sw = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto r = juce::Rectangle<float> ((float) i * sw, 0, sw, (float) getHeight()).reduced (3.0f, 2.0f);
        const bool on = i == index, hi = i == hover;
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (1.5f, 2.0f), 3.0f);
        if (on)
        {
            juce::Path pth; pth.addRoundedRectangle (r, 3.0f);
            juce::DropShadow (pal::signalHot.withAlpha (0.4f), 10, {}).drawForPath (g, pth);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a2a14), r.getCentreX(), r.getY(), juce::Colour (0xff3a0e06), r.getCentreX(), r.getBottom(), false));
        }
        else
            g.setGradientFill (juce::ColourGradient (hi ? juce::Colour (0xff43332a) : juce::Colour (0xff332822), r.getCentreX(), r.getY(),
                                                     juce::Colour (0xff15100d), r.getCentreX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? pal::signalHot : (hi ? pal::rustLight : juce::Colour (0xff5a3420)));
        g.drawRoundedRectangle (r, 3.0f, on ? 1.6f : 1.1f);
        g.setColour (on ? juce::Colour (0xfffff0d8) : pal::paint.withAlpha (0.85f));
        g.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.08f));
        g.drawFittedText (labels[i], r.reduced (6, 2).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

//==============================================================================
void FreakEye::advance (float activityDb, float dt)
{
    t += dt;
    heat += (juce::jlimit (0.0f, 1.0f, activityDb / 6.0f) - heat) * (activityDb / 6.0f > heat ? 0.35f : 0.04f);
    auto& rnd = juce::Random::getSystemRandom();
    nextLook -= dt * (1.0f + heat * 4.0f);       // a busy freak darts about
    if (nextLook <= 0)
    {
        targetX = (rnd.nextFloat() * 2.0f - 1.0f) * 0.55f;
        targetY = (rnd.nextFloat() * 2.0f - 1.0f) * 0.3f;
        nextLook = 0.6f + rnd.nextFloat() * 2.5f;
    }
    lookX += (targetX - lookX) * 0.25f;
    lookY += (targetY - lookY) * 0.25f;
    if (blink <= 0 && rnd.nextFloat() < dt * 0.25f) blink = 1.0f;
    if (blink > 0) blink = std::max (0.0f, blink - dt * 7.0f);
}

void FreakEye::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = r.getCentre();
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;

    // iron medallion
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (r.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff1a1410), c.x + R, c.y + R, true));
    g.fillEllipse (r);
    g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (r.reduced (1.0f), 1.6f);
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i / 8.0f * juce::MathConstants<float>::twoPi + 0.39f;
        const float rx = c.x + std::cos (a) * R * 0.88f, ry = c.y + std::sin (a) * R * 0.88f;
        g.setColour (juce::Colour (0xff8a7a68)); g.fillEllipse (rx - 1.8f, ry - 1.8f, 3.6f, 3.6f);
    }

    // the eye
    const float ew = R * 1.45f, eh = R * 0.95f * (1.0f - 0.9f * std::sin (blink * juce::MathConstants<float>::pi));
    juce::Path lid;
    lid.startNewSubPath (c.x - ew * 0.5f, c.y);
    lid.quadraticTo (c.x, c.y - eh, c.x + ew * 0.5f, c.y);
    lid.quadraticTo (c.x, c.y + eh, c.x - ew * 0.5f, c.y);
    lid.closeSubPath();
    if (heat > 0.05f) juce::DropShadow (pal::signalHot.withAlpha (0.8f * heat), (int) (R * 0.6f), {}).drawForPath (g, lid);
    g.setColour (juce::Colour (0xff0a0706)); g.fillPath (lid);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (lid);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8dcc0).interpolatedWith (juce::Colour (0xffffb0a0), heat * 0.6f), c.x, c.y - eh * 0.3f,
                                                 juce::Colour (0xff8a7058).interpolatedWith (juce::Colour (0xff901808), heat), c.x + ew * 0.5f, c.y + eh * 0.5f, true));
        g.fillPath (lid);
        // bloodshot veins
        juce::Random vr (31);
        g.setColour (juce::Colour (0xffb01808).withAlpha (0.25f + 0.65f * heat));
        for (int i = 0; i < 9; ++i)
        {
            const bool left = i % 2 == 0;
            float x = c.x + (left ? -1.0f : 1.0f) * ew * 0.5f, y = c.y + (vr.nextFloat() - 0.5f) * eh * 0.8f;
            juce::Path v; v.startNewSubPath (x, y);
            for (int k = 0; k < 4; ++k)
            {
                x += (left ? 1.0f : -1.0f) * ew * (0.05f + vr.nextFloat() * 0.06f);
                y += (vr.nextFloat() - 0.5f) * eh * 0.25f;
                v.lineTo (x, y);
            }
            g.strokePath (v, juce::PathStrokeType (0.7f + heat * 0.6f));
        }
        // iris + pupil
        const float ir = R * 0.36f;
        const float ix = c.x + lookX * (ew * 0.5f - ir), iy = c.y + lookY * (eh * 0.4f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffa040).interpolatedWith (pal::signalHot, heat), ix, iy,
                                                 juce::Colour (0xff5a1a06), ix + ir, iy + ir, true));
        g.fillEllipse (ix - ir, iy - ir, ir * 2, ir * 2);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        for (int i = 0; i < 18; ++i)
        {
            const float a = (float) i / 18.0f * juce::MathConstants<float>::twoPi;
            g.drawLine (ix + std::cos (a) * ir * 0.45f, iy + std::sin (a) * ir * 0.45f, ix + std::cos (a) * ir, iy + std::sin (a) * ir, 0.6f);
        }
        const float pr = ir * (0.35f + 0.45f * heat);     // dilates as it freaks out
        g.setColour (juce::Colour (0xff050303)); g.fillEllipse (ix - pr, iy - pr, pr * 2, pr * 2);
        g.setColour (juce::Colours::white.withAlpha (0.8f)); g.fillEllipse (ix - ir * 0.45f, iy - ir * 0.55f, ir * 0.28f, ir * 0.28f);
    }
    g.setColour (juce::Colour (0xff1a0e0a)); g.strokePath (lid, juce::PathStrokeType (1.6f));
}

//==============================================================================
CurveView::CurveView (FreakProcessor& p) : proc (p)
{
    setTooltip ("Drag a band to set its frequency and gain. Cmd/Ctrl-drag (or Alt-drag) up and down for DEPTH. "
                "Scroll for width (Q). Double-click a band to switch it on or off.");
}

juce::RangedAudioParameter* CurveView::param (int band, const char* what) const { return proc.apvts.getParameter (FreakProcessor::bandId (band, what)); }
float CurveView::value (int band, const char* what) const { return proc.apvts.getRawParameterValue (FreakProcessor::bandId (band, what))->load(); }
void CurveView::setValue (int band, const char* what, float v)
{
    if (auto* p = param (band, what)) p->setValueNotifyingHost (p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (v)));
}

float CurveView::xForFreq (float f) const { auto a = plotArea(); return a.getX() + a.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
float CurveView::freqForX (float x) const { auto a = plotArea(); return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - a.getX()) / a.getWidth())); }
float CurveView::yForDb (float db) const { auto a = plotArea(); return a.getCentreY() - db / 18.0f * a.getHeight() * 0.5f; }
float CurveView::dbForY (float y) const { auto a = plotArea(); return (a.getCentreY() - y) / (a.getHeight() * 0.5f) * 18.0f; }

juce::Point<float> CurveView::nodePos (int b) const
{
    const float g = value (b, "g") + (value (b, "on") > 0.5f ? dyn[(size_t) b] : 0.0f);
    return { xForFreq (value (b, "f")), yForDb (juce::jlimit (-18.0f, 18.0f, g)) };
}

int CurveView::nodeAt (juce::Point<float> pt) const
{
    // selected band wins ties
    if (nodePos (selected).getDistanceFrom (pt) < 12.0f) return selected;
    int best = -1; float bd = 12.0f;
    for (int b = 0; b < freak::kBands; ++b)
    {
        const float d = nodePos (b).getDistanceFrom (pt);
        if (d < bd) { bd = d; best = b; }
    }
    return best;
}

void CurveView::mouseMove (const juce::MouseEvent& e) { const int h = nodeAt (e.position); if (h != hover) { hover = h; repaint(); } }

void CurveView::mouseDown (const juce::MouseEvent& e)
{
    dragging = nodeAt (e.position);
    if (dragging < 0) return;
    selected = dragging;
    if (onSelect) onSelect (dragging);
    dragDepth = e.mods.isCommandDown() || e.mods.isAltDown();
    dragStartDepth = value (dragging, "d");
    dragStartY = e.position.y;
    for (auto* w : { "f", "g", "d", "on" }) if (auto* p = param (dragging, w)) p->beginChangeGesture();
}

void CurveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0 || e.getDistanceFromDragStart() < 2) return;
    if (value (dragging, "on") < 0.5f) setValue (dragging, "on", 1.0f);   // dragging a sleeping band wakes it
    if (dragDepth)
        setValue (dragging, "d", juce::jlimit (-18.0f, 12.0f, dragStartDepth + (dragStartY - e.position.y) * 0.12f));
    else
    {
        setValue (dragging, "f", juce::jlimit (20.0f, 20000.0f, freqForX (e.position.x)));
        setValue (dragging, "g", juce::jlimit (-12.0f, 12.0f, dbForY (e.position.y)));
    }
    repaint();
}

void CurveView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0) for (auto* w : { "f", "g", "d", "on" }) if (auto* p = param (dragging, w)) p->endChangeGesture();
    dragging = -1;
}

void CurveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = nodeAt (e.position);
    if (b < 0) return;
    if (auto* p = param (b, "on")) p->setValueNotifyingHost (value (b, "on") > 0.5f ? 0.0f : 1.0f);
}

void CurveView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = nodeAt (e.position);
    if (b < 0) b = selected;
    if (auto* p = param (b, "q"))
    {
        p->beginChangeGesture();
        setValue (b, "q", value (b, "q") * std::exp (w.deltaY * (w.isReversed ? -0.8f : 0.8f)));
        p->endChangeGesture();
    }
}

void CurveView::pullSpectrum()
{
    auto& f = proc.specFifo;
    const int avail = f.getNumReady();
    if (avail > 0)
    {
        const auto r = f.read (avail);
        auto take = [&] (int start, int count) { for (int i = 0; i < count; ++i) { ring[(size_t) ringPos] = proc.specBuf[(size_t) (start + i)]; ringPos = (ringPos + 1) % kSize; } };
        take (r.startIndex1, r.blockSize1);
        take (r.startIndex2, r.blockSize2);
    }
    for (int i = 0; i < kSize; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kSize - 1));
        work[(size_t) i] = ring[(size_t) ((ringPos + i) % kSize)] * w;
    }
    std::fill (work.begin() + kSize, work.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (work.data(), true);
    for (int i = 0; i < kSize / 2; ++i)
    {
        const float db = juce::Decibels::gainToDecibels (work[(size_t) i] * 2.0f / (float) (kSize / 2), -120.0f);
        auto& s = spec[(size_t) i];
        s = db > s ? s + (db - s) * 0.6f : std::max (db, s - 1.2f);
    }
}

void CurveView::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setColour (pal::screen); g.fillRoundedRectangle (full, 4.0f);
    const auto a = plotArea();
    double fs = proc.getEngine().getSampleRate();
    if (fs < 8000.0) fs = 48000.0;
    const float amount = proc.apvts.getRawParameterValue ("amount")->load() * 0.01f;

    // grid
    g.setFont (tex::plainBold (9.0f));
    for (float fq : { 30.0f, 50.0f, 100.0f, 200.0f, 300.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = xForFreq (fq);
        const bool major = fq == 100.0f || fq == 1000.0f || fq == 10000.0f;
        g.setColour (pal::paintDim.withAlpha (major ? 0.16f : 0.08f)); g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (pal::paintDim.withAlpha (0.6f));
        g.drawText (bandpal::freqText (fq), (int) x - 20, (int) a.getBottom() + 2, 40, 12, juce::Justification::centred);
    }
    for (int db : { -18, -12, -6, 0, 6, 12, 18 })
    {
        const float y = yForDb ((float) db);
        g.setColour (pal::paintDim.withAlpha (db == 0 ? 0.25f : 0.08f)); g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (pal::paintDim.withAlpha (0.6f));
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), (int) full.getX() + 2, (int) y - 6, 26, 12, juce::Justification::centredRight);
    }

    juce::Graphics::ScopedSaveState clipState (g);
    g.reduceClipRegion (a.toNearestInt());

    // spectrum
    {
        juce::Path sp;
        sp.startNewSubPath (a.getX(), a.getBottom());
        for (float x = a.getX(); x <= a.getRight(); x += 2.0f)
        {
            const float f0 = freqForX (x), f1 = freqForX (x + 2.0f);
            const int b0 = juce::jlimit (1, kSize / 2 - 1, (int) (f0 / fs * kSize)), b1 = juce::jlimit (b0, kSize / 2 - 1, (int) (f1 / fs * kSize));
            float m = -120.0f;
            for (int i = b0; i <= b1; ++i) m = std::max (m, spec[(size_t) i]);
            // +3 dB/oct tilt so a balanced mix reads flat-ish
            m += 3.0f * std::log2 (std::max (20.0f, f0) / 1000.0f);
            const float y = a.getBottom() - juce::jlimit (0.0f, 1.0f, (m + 78.0f) / 78.0f) * a.getHeight();
            sp.lineTo (x, y);
        }
        sp.lineTo (a.getRight(), a.getBottom());
        sp.closeSubPath();
        g.setGradientFill (juce::ColourGradient (pal::paint.withAlpha (0.16f), 0, a.getY(), pal::paint.withAlpha (0.03f), 0, a.getBottom(), false));
        g.fillPath (sp);
        g.setColour (pal::paint.withAlpha (0.22f)); g.strokePath (sp, juce::PathStrokeType (1.0f));
    }

    // curves
    struct BandC { bool on; freak::Coefs stat, live, reach; };
    std::array<BandC, freak::kBands> bc;
    for (int b = 0; b < freak::kBands; ++b)
    {
        const int type = (int) value (b, "type");
        const float f = value (b, "f"), q = value (b, "q"), gn = value (b, "g"), d = value (b, "d") * amount;
        bc[(size_t) b] = { value (b, "on") > 0.5f, freak::makeEq (type, fs, f, q, gn), freak::makeEq (type, fs, f, q, gn + dyn[(size_t) b]),
                           freak::makeEq (type, fs, f, q, gn + d) };
    }
    auto curveFor = [&] (std::function<double (double)> fn)
    {
        juce::Path p;
        for (float x = a.getX(); x <= a.getRight() + 1.0f; x += 2.0f)
        {
            const float y = yForDb ((float) fn (freqForX (x)));
            if (x == a.getX()) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };

    for (int b = 0; b < freak::kBands; ++b)
    {
        const auto& c = bc[(size_t) b];
        if (! c.on) continue;
        const auto col = bandpal::col (b);
        if (b == selected)
        {
            // the reach: where this band can go when it moves
            auto stat = curveFor ([&] (double f) { return freak::magnitudeDb (c.stat, fs, f); });
            auto reach = curveFor ([&] (double f) { return freak::magnitudeDb (c.reach, fs, f); });
            juce::Path area (reach);
            for (float x = a.getRight() + 1.0f; x >= a.getX() - 1.0f; x -= 2.0f) area.lineTo (x, yForDb ((float) freak::magnitudeDb (c.stat, fs, freqForX (x))));
            area.closeSubPath();
            g.setColour (col.withAlpha (0.16f)); g.fillPath (area);
            g.setColour (col.withAlpha (0.55f));
            juce::Path dashed; const float dl[] = { 4.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, reach, dl, 2);
            g.fillPath (dashed);
            g.setColour (col.withAlpha (0.45f)); g.strokePath (stat, juce::PathStrokeType (1.0f));
        }
        else
        {
            g.setColour (col.withAlpha (0.3f));
            g.strokePath (curveFor ([&] (double f) { return freak::magnitudeDb (c.live, fs, f); }), juce::PathStrokeType (1.0f));
        }
    }
    auto totalStat = curveFor ([&] (double f) { double s = 0; for (auto& c : bc) if (c.on) s += freak::magnitudeDb (c.stat, fs, f); return s; });
    auto totalLive = curveFor ([&] (double f) { double s = 0; for (auto& c : bc) if (c.on) s += freak::magnitudeDb (c.live, fs, f); return s; });
    g.setColour (pal::paintDim.withAlpha (0.5f)); g.strokePath (totalStat, juce::PathStrokeType (1.2f));
    {
        juce::Path glow;
        juce::PathStrokeType (2.0f).createStrokedPath (glow, totalLive);
        juce::DropShadow (pal::signalHot.withAlpha (0.7f), 8, {}).drawForPath (g, glow);
    }
    g.setColour (juce::Colour (0xfffff0d8)); g.strokePath (totalLive, juce::PathStrokeType (2.0f));

    // nodes
    for (int pass = 0; pass < 2; ++pass)
        for (int b = 0; b < freak::kBands; ++b)
        {
            if ((b == selected) != (pass == 1)) continue;   // selected on top
            const bool on = bc[(size_t) b].on;
            const auto col = bandpal::col (b);
            const auto pos = nodePos (b);
            const float rad = b == selected ? 10.0f : 8.5f;
            if (on && std::abs (dyn[(size_t) b]) > 0.05f)
            {
                // tether from the resting spot to where the band has been pushed
                const auto rest = juce::Point<float> (pos.x, yForDb (value (b, "g")));
                g.setColour (col.withAlpha (0.7f)); g.drawLine ({ rest, pos }, 1.5f);
                g.drawEllipse (juce::Rectangle<float> (8, 8).withCentre (rest), 1.0f);
            }
            auto r = juce::Rectangle<float> (rad * 2, rad * 2).withCentre (pos);
            if (b == selected || b == hover) { juce::Path pp; pp.addEllipse (r); juce::DropShadow (col.withAlpha (0.8f), 10, {}).drawForPath (g, pp); }
            g.setColour (on ? col : juce::Colour (0xff1a1411)); g.fillEllipse (r);
            g.setColour (on ? juce::Colours::black.withAlpha (0.6f) : col.withAlpha (0.8f)); g.drawEllipse (r, b == selected ? 2.0f : 1.2f);
            if (b == selected) { g.setColour (juce::Colours::white.withAlpha (0.85f)); g.drawEllipse (r.expanded (3.0f), 1.2f); }
            g.setColour (on ? juce::Colour (0xff120c0a) : col);
            g.setFont (tex::stencil (13.0f));
            g.drawText (juce::String (b + 1), r.toNearestInt(), juce::Justification::centred);
        }

    g.setColour (pal::paintDim.withAlpha (0.55f));
    g.setFont (tex::plainBold (9.0f).withExtraKerningFactor (0.08f));
    g.drawText ("DRAG: FREQ + GAIN   CMD-DRAG: DEPTH   SCROLL: WIDTH   DOUBLE-CLICK: ON / OFF",
                a.toNearestInt().reduced (8, 4), juce::Justification::topRight);
}

//==============================================================================
void BandTabs::mouseDown (const juce::MouseEvent& e) { selected = rowAt (e.y); if (onSelect) onSelect (selected); repaint(); }
void BandTabs::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = rowAt (e.y);
    if (auto* p = proc.apvts.getParameter (FreakProcessor::bandId (b, "on"))) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
}

void BandTabs::paint (juce::Graphics& g)
{
    const float rh = (float) getHeight() / (float) freak::kBands;
    for (int b = 0; b < freak::kBands; ++b)
    {
        auto v = [&] (const char* w) { return proc.apvts.getRawParameterValue (FreakProcessor::bandId (b, w))->load(); };
        const bool on = v ("on") > 0.5f, sel = b == selected;
        const auto col = bandpal::col (b);
        auto r = juce::Rectangle<float> (0, (float) b * rh, (float) getWidth(), rh).reduced (1.0f, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
        g.setGradientFill (juce::ColourGradient (sel ? juce::Colour (0xff3e2418) : juce::Colour (0xff2a221d), 0, r.getY(), juce::Colour (0xff120d0b), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (sel ? col : juce::Colour (0xff4a2c1c)); g.drawRoundedRectangle (r, 3.0f, sel ? 1.5f : 1.0f);

        auto lamp = juce::Rectangle<float> (10, 10).withCentre ({ r.getX() + 12, r.getCentreY() });
        if (on) { juce::Path lp; lp.addEllipse (lamp); juce::DropShadow (col.withAlpha (0.9f), 8, {}).drawForPath (g, lp); }
        g.setColour (on ? col : juce::Colour (0xff2a1c16)); g.fillEllipse (lamp);
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (lamp, 1.0f);

        g.setColour (on ? pal::paint : pal::paintDim.withAlpha (0.7f));
        g.setFont (tex::stencil (15.0f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (r.getX() + 22, r.getY(), 14, r.getHeight()).toNearestInt(), juce::Justification::centred);
        g.setFont (tex::plainBold (10.0f));
        const int type = (int) v ("type");
        g.drawText ((type == freak::bell ? "BELL " : (type == freak::lowShelf ? "LO SH " : "HI SH ")) + bandpal::freqText (v ("f")),
                    juce::Rectangle<float> (r.getX() + 40, r.getY(), 70, r.getHeight()).toNearestInt(), juce::Justification::centredLeft);

        // live move: a little bar centred on zero, cut to the left, lift to the right
        auto bar = juce::Rectangle<float> (r.getRight() - 64, r.getCentreY() - 3, 56, 6);
        g.setColour (juce::Colour (0xff0c0908)); g.fillRect (bar);
        g.setColour (pal::paintDim.withAlpha (0.4f)); g.fillRect (bar.getCentreX() - 0.5f, bar.getY() - 2, 1.0f, bar.getHeight() + 4);
        const float d = on ? dyn[(size_t) b] : 0.0f;
        const float w = juce::jlimit (0.0f, 1.0f, std::abs (d) / 12.0f) * bar.getWidth() * 0.5f;
        g.setColour (col);
        if (d < 0) g.fillRect (bar.getCentreX() - w, bar.getY(), w, bar.getHeight());
        else       g.fillRect (bar.getCentreX(), bar.getY(), w, bar.getHeight());
    }
}

//==============================================================================
void LevelBars::paint (juce::Graphics& g)
{
    auto m = getLocalBounds().toFloat();
    g.setColour (pal::screen); g.fillRoundedRectangle (m, 3.0f);
    auto rows = m.reduced (6.0f, 6.0f);
    auto drawRow = [&] (juce::Rectangle<float> row, float lvl, const juce::String& name)
    {
        g.setColour (pal::paint); g.setFont (tex::stencil (12.0f));
        g.drawText (name, row.removeFromLeft (30).toNearestInt(), juce::Justification::centredLeft);
        const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
        auto txt = row.removeFromRight (36);
        g.setColour (pal::paintDim); g.setFont (tex::plainBold (9.5f));
        g.drawText (lvl > 0.00002f ? juce::String (db, 1) : juce::String ("-inf"), txt.toNearestInt(), juce::Justification::centredRight);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        const int segs = 24;
        const float sw = row.getWidth() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) (i + 1) / (float) segs;
            auto seg = juce::Rectangle<float> (row.getX() + (float) i * sw + 0.5f, row.getY() + 3, sw - 1.5f, row.getHeight() - 6);
            const auto c = t > 0.94f ? pal::signalHot : (t > 0.8f ? pal::hazard : pal::paint.darker (0.15f));
            g.setColour (t <= frac ? c : c.withAlpha (0.07f));
            g.fillRect (seg);
        }
    };
    drawRow (rows.removeFromTop (rows.getHeight() * 0.5f), in, "IN");
    drawRow (rows, out, "OUT");
}

//==============================================================================
FreakEditor::FreakEditor (FreakProcessor& p)
    : AudioProcessorEditor (&p), proc (p), curve (p), tabs (p)
{
    setLookAndFeel (&lnf);

    freqKnob  = &makeKnob ("FREQ");      freqKnob->suffix = "f";
    qKnob     = &makeKnob ("WIDTH (Q)"); qKnob->suffix = "q";
    gainKnob  = &makeKnob ("GAIN");      gainKnob->suffix = "g";
    depthKnob = &makeKnob ("DEPTH");     depthKnob->suffix = "d";
    thrKnob   = &makeKnob ("THRESHOLD"); thrKnob->suffix = "t";
    gainKnob->slider.getProperties().set ("bip", true);
    depthKnob->slider.getProperties().set ("bip", true);
    amountKnob = &makeKnob ("AMOUNT");   bindKnob (*amountKnob, "amount");
    speedKnob  = &makeKnob ("SPEED");    bindKnob (*speedKnob, "speed");
    outKnob    = &makeKnob ("OUTPUT");   bindKnob (*outKnob, "output");

    freqKnob->slider.setTooltip ("Where this band sits (the corner frequency for a shelf).");
    qKnob->slider.setTooltip ("How wide the band is. Higher = narrower. For a shelf it sets the slope.");
    gainKnob->slider.setTooltip ("Static boost or cut. Always applied, like a normal EQ.");
    depthKnob->slider.setTooltip ("How far the band may move on its own. Negative: cuts when this range jumps above its normal level. "
                                  "Positive: lifts it when it sinks below. 0 = a plain static EQ band.");
    thrKnob->slider.setTooltip ("How far (dB) this range has to stray from its own recent average before the band moves.");
    amountKnob->slider.setTooltip ("Scales every band's DEPTH at once. 0% = static EQ only, 200% = twice as freaky.");
    speedKnob->slider.setTooltip ("How fast the bands react and let go. Slow for smoothing, fast for catching hits.");
    outKnob->slider.setTooltip ("Output level.");

    addAndMakeVisible (typeSel);
    typeSel.setTooltip ("Bell, or a shelf that moves everything below (LOW) or above (HIGH) the frequency.");
    typeSel.onChange = [this] { tabs.repaint(); repaint (bandTitleArea); };
    addAndMakeVisible (onButton);
    addAndMakeVisible (listenButton);
    listenButton.setName ("listen");
    listenButton.setTooltip ("Hear only what this band is listening to, to find the problem. Turn it off before you bounce!");
    listenButton.onClick = [this]
    {
        if (auto* lp = proc.apvts.getParameter ("listen"))
            lp->setValueNotifyingHost (lp->convertTo0to1 (listenButton.getToggleState() ? (float) (selected + 1) : 0.0f));
    };

    addAndMakeVisible (eye);
    eye.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (curve);
    curve.onSelect = [this] (int b) { selectBand (b); };
    addAndMakeVisible (tabs);
    tabs.onSelect = [this] (int b) { selectBand (b); };
    addAndMakeVisible (levels);
    levels.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (noteLabel);
    noteLabel.setFont (tex::plainBold (10.5f));
    noteLabel.setColour (juce::Label::textColourId, pal::hazard);
    noteLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (presetBox);
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id >= kUserIdBase)
        {
            if (id - kUserIdBase < userFiles.size()) proc.loadUserPreset (userFiles[id - kUserIdBase]);
        }
        else if (id - 1 != proc.getCurrentProgram() || proc.getCurrentPresetName() != proc.getProgramName (id - 1))
            proc.setCurrentProgram (id - 1);
    };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &deleteButton }) addAndMakeVisible (*b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { promptSave(); };
    deleteButton.onClick = [this] { confirmDelete(); };

    // open on the first band that's switched on
    int first = 0;
    for (int b = freak::kBands - 1; b >= 0; --b)
        if (proc.apvts.getRawParameterValue (FreakProcessor::bandId (b, "on"))->load() > 0.5f) first = b;
    selectBand (first);
    refreshPresetBox();
    addAndMakeVisible (rebel);
    setSize (980, 650);
    startTimerHz (30);
}

FreakEditor::~FreakEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

FreakEditor::Knob& FreakEditor::makeKnob (const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (tex::stencil (14.0f).withExtraKerningFactor (0.08f));
    k->label.setColour (juce::Label::textColourId, pal::paint);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 17);
    k->slider.setColour (juce::Slider::textBoxTextColourId, pal::paint.withAlpha (0.85f));
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a2418));
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x90000000));
    k->slider.setMouseDragSensitivity (220);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void FreakEditor::bindKnob (Knob& k, const juce::String& id)
{
    k.att.reset();
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    auto* p = proc.apvts.getParameter (id);
    k.slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void FreakEditor::placeKnob (Knob& k, juce::Rectangle<int> cell)
{
    k.label.setBounds (cell.removeFromTop (18));
    k.slider.setBounds (cell);
}

void FreakEditor::selectBand (int band)
{
    selected = juce::jlimit (0, freak::kBands - 1, band);
    curve.selected = selected;
    tabs.selected = selected;
    refreshBandPlate();
    curve.repaint();
    tabs.repaint();
}

void FreakEditor::refreshBandPlate()
{
    const auto col = bandpal::col (selected);
    for (auto* k : { freqKnob, qKnob, gainKnob, depthKnob, thrKnob })
    {
        bindKnob (*k, FreakProcessor::bandId (selected, k->suffix.toRawUTF8()));
        k->slider.getProperties().set ("col", (juce::int64) col.getARGB());
        k->slider.repaint();
    }
    typeSel.bind (*proc.apvts.getParameter (FreakProcessor::bandId (selected, "type")));
    onAtt.reset();
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, FreakProcessor::bandId (selected, "on"), onButton);
    listenButton.setToggleState ((int) proc.apvts.getRawParameterValue ("listen")->load() == selected + 1, juce::dontSendNotification);
    noteLabel.setText (proc.bandNotes[selected], juce::dontSendNotification);
    repaint (bandTitleArea);
}

void FreakEditor::resized()
{
    plates.clear();
    logoArea  = { 18, 12, 460, 92 };
    eye.setBounds (18, 12, 90, 90);
    badgeArea = { 494, 14, 252, 86 };
    crestArea = { 502, 19, 76, 76 };
    presetArea = { 760, 14, 204, 86 };
    presetBox.setBounds (760, 36, 204, 26);
    prevButton.setBounds (760, 70, 28, 24);
    nextButton.setBounds (792, 70, 28, 24);
    saveButton.setBounds (826, 70, 62, 24);
    deleteButton.setBounds (894, 70, 70, 24);
    stripeArea = { 0, 110, getWidth(), 10 };
    linkArea = { 300, 84, 180, 14 };
    rebel.setBounds (linkArea.getX(), linkArea.getCentreY() - 8, 54, 16);

    plates.push_back ({ "THE CURVE", { 16, 130, 948, 290 } });
    curve.setBounds (28, 154, 924, 256);

    plates.push_back ({ "BANDS", { 16, 430, 204, 204 } });
    tabs.setBounds (26, 454, 184, 170);

    plates.push_back ({ "", { 230, 430, 516, 204 }, true });
    bandTitleArea = { 244, 431, 480, 20 };
    onButton.setBounds (242, 456, 78, 30);
    typeSel.setBounds (328, 455, 280, 32);
    listenButton.setBounds (616, 456, 120, 30);
    const int kx = 238, kw = 100, ky = 492, kh = 118;
    placeKnob (*freqKnob,  { kx,          ky, kw, kh });
    placeKnob (*qKnob,     { kx + kw,     ky, kw, kh });
    placeKnob (*gainKnob,  { kx + kw * 2, ky, kw, kh });
    placeKnob (*depthKnob, { kx + kw * 3, ky, kw, kh });
    placeKnob (*thrKnob,   { kx + kw * 4, ky, kw, kh });
    noteLabel.setBounds (242, 610, 496, 18);

    plates.push_back ({ "GLOBAL", { 756, 430, 208, 204 } });
    placeKnob (*amountKnob, { 762, 452, 66, 112 });
    placeKnob (*speedKnob,  { 828, 452, 66, 112 });
    placeKnob (*outKnob,    { 894, 452, 66, 112 });
    levels.setBounds (766, 572, 188, 52);

    buildBackdrop();
}

//==============================================================================
void FreakEditor::buildBackdrop()
{
    const float scale = 2.0f;
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;
    const int PW = (int) ((float) W * scale), PH = (int) ((float) H * scale);
    juce::Image base (juce::Image::RGB, W, H, false);

    std::vector<juce::Rectangle<float>> pr;
    for (auto& s : plates) pr.push_back (s.bounds.toFloat());
    pr.push_back (badgeArea.toFloat());
    const auto stripe = stripeArea.toFloat();

    {
        juce::Image::BitmapData bd (base, juce::Image::BitmapData::writeOnly);
        for (int py = 0; py < H; ++py)
        {
            const float y = (float) py;
            for (int px = 0; px < W; ++px)
            {
                const float x = (float) px;
                tex::RGB col;
                int inside = -1; float edge = 1e9f; bool shadow = false;
                for (int i = 0; i < (int) pr.size(); ++i)
                {
                    const auto& p = pr[(size_t) i];
                    if (p.contains (x, y))
                    {
                        inside = i;
                        edge = std::min ({ x - p.getX(), p.getRight() - x, y - p.getY(), p.getBottom() - y });
                        break;
                    }
                    if (p.translated (3.0f, 4.0f).contains (x, y)) shadow = true;
                }
                if (stripe.contains (x, y))
                {
                    // worn hazard stripes
                    const bool yellow = std::fmod (x + y * 1.0f + 1000.0f, 28.0f) < 14.0f;
                    const float wear = tex::fbm (x / 12.0f, y / 4.0f, 2024, 3);
                    col = yellow ? tex::rgb (pal::hazard) : tex::rgb (juce::Colour (0xff141010));
                    col = tex::mix (col, tex::rustAt (x, y, 0.3f, 99), tex::smoothstep (0.5f, 0.68f, wear));
                    col = tex::mul (col, 0.85f + 0.15f * tex::hash (px, py, 3));
                }
                else if (inside >= 0)
                {
                    // dark diamond-plate steel, rust creeping in from the edges
                    const float d = tex::fbm (x / 30.0f, y / 30.0f, 211, 3);
                    tex::RGB steel = tex::mix (tex::rgb (juce::Colour (0xff1e1a17)), tex::rgb (juce::Colour (0xff332c27)), d);
                    // diamond tread
                    const float u = std::fmod (x + y + 2000.0f, 16.0f) / 16.0f, v = std::fmod (x - y + 2000.0f, 16.0f) / 16.0f;
                    const float tread = (std::abs (u - 0.5f) < 0.08f && std::abs (v - 0.25f) < 0.18f) || (std::abs (v - 0.5f) < 0.08f && std::abs (u - 0.75f) < 0.18f) ? 1.0f : 0.0f;
                    steel = tex::mix (steel, tex::rgb (juce::Colour (0xff4a4038)), tread * 0.55f);
                    const float edgeRust = (1.0f - juce::jlimit (0.0f, 1.0f, edge / 14.0f)) * tex::smoothstep (0.35f, 0.6f, tex::vnoise (x / 6.0f, y / 6.0f, 17));
                    const float bloom = tex::smoothstep (0.6f, 0.75f, tex::fbm (x / 55.0f, y / 55.0f, 404, 3)) * 0.7f;
                    col = tex::mix (steel, tex::rustAt (x, y, 0.15f, 1337), juce::jlimit (0.0f, 1.0f, edgeRust + bloom));
                    col = tex::mul (col, 0.88f);
                }
                else
                {
                    col = tex::rustAt (x, y, 0.1f, 42);
                    if (shadow) col = tex::mul (col, 0.45f);
                }

                const float streak = tex::fbm (x / 9.0f, y / 240.0f, 909, 3);
                const float drip = tex::smoothstep (0.58f, 0.78f, streak) * (0.25f + 0.5f * y / (float) H);
                col = tex::mul (col, 1.0f - drip * 0.6f);
                const float runoff = tex::smoothstep (0.66f, 0.82f, tex::fbm (x / 5.0f, y / 120.0f, 31337, 2));
                col = tex::mix (col, tex::rgb (pal::rustDeep), runoff * 0.35f);
                // oil stains
                const float oil = tex::smoothstep (0.7f, 0.8f, tex::fbm (x / 70.0f, y / 50.0f, 555, 3));
                col = tex::mul (col, 1.0f - oil * 0.45f);
                if (tex::hash (px, py, 5) > 0.988f) col = tex::mul (col, 0.45f);
                const float grime = 0.72f + 0.28f * tex::fbm (x / 260.0f, y / 260.0f, 77, 2);
                const float dx = (x / (float) W - 0.5f) * 2.0f, dy = (y / (float) H - 0.5f) * 2.0f;
                const float vig = 1.0f - 0.45f * std::min (1.0f, (dx * dx + dy * dy) * 0.6f);
                col = tex::mul (col, grime * vig);
                bd.setPixelColour (px, py, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, col.r), juce::jlimit (0.0f, 1.0f, col.g),
                                                                        juce::jlimit (0.0f, 1.0f, col.b), 1.0f));
            }
        }
    }

    backdrop = juce::Image (juce::Image::RGB, PW, PH, false);
    juce::Graphics g (backdrop);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (base, 0, 0, PW, PH, 0, 0, W, H);
    g.addTransform (juce::AffineTransform::scale (scale));
    juce::Random rnd (1991);

    auto rivet = [&] (float cx, float cy, float rad)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - rad + 1, cy - rad + 1.5f, rad * 2, rad * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a7a68), cx - rad * 0.4f, cy - rad * 0.5f, juce::Colour (0xff2a201a), cx + rad, cy + rad, true));
        g.fillEllipse (cx - rad, cy - rad, rad * 2, rad * 2);
        g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (cx - rad, cy - rad, rad * 2, rad * 2, 1.2f);
        g.setColour (pal::rustDeep.withAlpha (0.35f)); g.fillRect (cx - 0.8f, cy + rad, 1.6f, 3.0f + rnd.nextFloat() * 9.0f);
    };
    auto plateFrame = [&] (juce::Rectangle<float> p, bool hot)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawLine (p.getX() + 2, p.getY() + 1, p.getRight() - 2, p.getY() + 1, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRect (p, 1.0f);
        if (hot) { g.setColour (pal::signal.withAlpha (0.45f)); g.drawRect (p.reduced (2.0f), 1.5f); }
        rivet (p.getX() + 6, p.getY() + 6, 3.0f); rivet (p.getRight() - 6, p.getY() + 6, 3.0f);
        rivet (p.getX() + 6, p.getBottom() - 6, 3.0f); rivet (p.getRight() - 6, p.getBottom() - 6, 3.0f);
    };

    // scratches + weld spatter
    for (int i = 0; i < 300; ++i)
    {
        const float x = rnd.nextFloat() * (float) W, y = rnd.nextFloat() * (float) H;
        const float len = 6.0f + rnd.nextFloat() * 70.0f, ang = rnd.nextFloat() * juce::MathConstants<float>::pi;
        g.setColour ((rnd.nextBool() ? juce::Colour (0xffd0c0a8) : juce::Colours::black).withAlpha (0.04f + rnd.nextFloat() * 0.1f));
        g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.5f + rnd.nextFloat() * 0.6f);
    }

    int seed = 10;
    for (auto& s : plates)
    {
        const auto p = s.bounds.toFloat();
        plateFrame (p, s.hot);
        auto title = tex::distressedText (s.title, s.bounds.getWidth() - 40, 18, tex::stencil (15.0f), s.hot ? pal::signalHot : pal::paint,
                                          juce::Justification::centredLeft, scale, 0.2f, seed++, 0.12f);
        g.drawImage (title, juce::Rectangle<float> (p.getX() + 14, p.getY() + 3, (float) s.bounds.getWidth() - 40, 18.0f));
    }
    // hazard stripe edges
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (0, stripe.getY(), (float) W, stripe.getY(), 1.5f);
    g.drawLine (0, stripe.getBottom(), (float) W, stripe.getBottom(), 1.5f);

    // CASTLE DANGER MUSIC COMPANY badge with the castle crest
    const auto badge = badgeArea.toFloat();
    plateFrame (badge, false);
    g.setColour (pal::paint.withAlpha (0.22f)); g.drawRect (badge.reduced (5.0f), 1.0f);
    drawCastleCrest (g, crestArea.toFloat());
    const float tx = (float) crestArea.getRight() + 8.0f, tw = badge.getRight() - tx - 10.0f;
    auto l1 = tex::distressedText ("CASTLE DANGER", (int) tw, 28, tex::stencil (25.0f), juce::Colour (0xffeadfc4), juce::Justification::centredLeft, scale, 0.24f, 4242, 0.08f);
    auto l2 = tex::distressedText ("MUSIC COMPANY", (int) tw, 20, tex::stencil (17.0f), pal::rustLight, juce::Justification::centredLeft, scale, 0.24f, 4343, 0.2f);
    auto l3 = tex::distressedText ("WE MAKE MUSIC", (int) tw, 14, tex::plainBold (9.0f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 4444, 0.3f);
    g.setColour (juce::Colours::black);
    g.setOpacity (0.7f); g.drawImage (l1, juce::Rectangle<float> (tx + 1.5f, badge.getY() + 13.0f, tw, 28.0f));
    g.setOpacity (1.0f); g.drawImage (l1, juce::Rectangle<float> (tx, badge.getY() + 11.0f, tw, 28.0f));
    g.setOpacity (0.7f); g.drawImage (l2, juce::Rectangle<float> (tx + 1.5f, badge.getY() + 40.0f, tw, 20.0f));
    g.setOpacity (1.0f); g.drawImage (l2, juce::Rectangle<float> (tx, badge.getY() + 38.0f, tw, 20.0f));
    g.drawImage (l3, juce::Rectangle<float> (tx + 1.0f, badge.getY() + 60.0f, tw, 14.0f));

    // stamped subtitle under the logo
    auto sub = tex::distressedText ("DYNAMIC EQ  //  V" + juce::String (JucePlugin_VersionString),
                                    170, 14, tex::plainBold (10.5f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 99, 0.14f);
    g.drawImage (sub, juce::Rectangle<float> (120.0f, 84.0f, 170.0f, 14.0f));

    // PRESET label
    auto pl = tex::distressedText ("PRESET", 120, 16, tex::stencil (14.0f), pal::paint, juce::Justification::centredLeft, scale, 0.2f, 7, 0.15f);
    g.drawImage (pl, juce::Rectangle<float> ((float) presetArea.getX() + 2, (float) presetArea.getY() + 2, 120.0f, 16.0f));

    // the logo: a small cream THE, and FREAK in rusty red with every letter knocked out of line
    {
        const int lw = 350, lh = 70;
        logoImage = juce::Image (juce::Image::ARGB, (int) (lw * scale), (int) (lh * scale), true);
        juce::Graphics lg (logoImage);
        lg.addTransform (juce::AffineTransform::scale (scale));
        lg.setColour (pal::paint);
        lg.setFont (tex::stencil (21.0f).withExtraKerningFactor (0.1f));
        lg.drawText ("THE", 2, 10, 60, 22, juce::Justification::centredLeft, false);
        auto f = tex::stencil (66.0f).withExtraKerningFactor (0.04f);
        const juce::String word ("FREAK");
        const float offs[] = { 3.0f, -4.0f, 2.0f, -2.0f, 5.0f }, rots[] = { -0.06f, 0.05f, -0.03f, 0.08f, -0.07f };
        float x = 60.0f;
        for (int i = 0; i < word.length(); ++i)
        {
            const auto ch = word.substring (i, i + 1);
            juce::GlyphArrangement ga; ga.addLineOfText (f, ch, 0, 0);
            const float cw = ga.getBoundingBox (0, -1, true).getWidth();
            juce::Graphics::ScopedSaveState ss (lg);
            lg.addTransform (juce::AffineTransform::rotation (rots[i], x + cw * 0.5f, (float) lh * 0.5f).translated (0.0f, offs[i]));
            lg.setFont (f);
            lg.setColour (i == 2 ? pal::signalHot : pal::signal);
            lg.drawText (ch, (int) x, 0, (int) cw + 4, lh, juce::Justification::centredLeft, false);
            x += cw + 4.0f;
        }
        tex::distress (logoImage, scale, 0.3f, 666);
    }
}


void FreakEditor::paint (juce::Graphics& g)
{
    if (backdrop.isValid()) g.drawImage (backdrop, getLocalBounds().toFloat());
    else g.fillAll (pal::ironDark);

    if (logoImage.isValid())
    {
        const float alpha = juce::jlimit (0.45f, 1.0f, 1.0f - flicker * 0.6f);
        const auto la = juce::Rectangle<float> (116.0f, 10.0f, 350.0f, 70.0f);
        g.setColour (juce::Colours::black);
        g.setOpacity (0.65f); g.drawImage (logoImage, la.translated (2.5f, 3.5f));
        g.setOpacity (alpha); g.drawImage (logoImage, la.translated (flicker > 0.5f ? 2.0f : 0.0f, 0.0f));
        g.setOpacity (1.0f);
    }

    // the band plate's title follows the selected band
    {
        auto r = bandTitleArea.toFloat();
        const int type = (int) proc.apvts.getRawParameterValue (FreakProcessor::bandId (selected, "type"))->load();
        g.setColour (bandpal::col (selected));
        g.fillEllipse (r.getX(), r.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.1f));
        g.setColour (pal::signalHot);
        g.drawText ("BAND " + juce::String (selected + 1) + "  //  " + bandpal::typeName (type), r.withTrimmedLeft (14).toNearestInt(), juce::Justification::centredLeft);
    }

    // Castle Danger Link status
    {
        auto& lk = proc.getLink();
        const bool linked = lk.masterNearby();
        const bool justSet = lk.getLastNoteTime() > 0 && juce::Time::getMillisecondCounter() - lk.getLastNoteTime() < 6000;
        const bool rebelOn = lk.isRebel();
        auto r = linkArea.toFloat().withTrimmedLeft (60);   // REBEL sits first on the line
        const auto col = rebelOn ? pal::signal : (justSet ? pal::signalHot : (linked ? pal::hazard : pal::paintDim.withAlpha (0.35f)));
        if (linked) { juce::Path lp; lp.addEllipse (r.getX(), r.getCentreY() - 3.5f, 7, 7); juce::DropShadow (col.withAlpha (0.8f), 6, {}).drawForPath (g, lp); }
        g.setColour (col); g.fillEllipse (r.getX(), r.getCentreY() - 3.5f, 7, 7);
        g.setFont (tex::plainBold (9.5f).withExtraKerningFactor (0.1f));
        g.drawText (rebelOn ? "HANDS OFF" : (justSet ? "JUST SET" : (linked ? "LINKED" : "LINK")), r.withTrimmedLeft (11).toNearestInt(), juce::Justification::centredLeft);
    }
}

void FreakEditor::timerCallback()
{
    if (++linkTick >= 15) { linkTick = 0; repaint (linkArea.expanded (8)); }
    auto& e = proc.getEngine();
    float activity = 0;
    for (int b = 0; b < freak::kBands; ++b)
    {
        const float d = e.dynDb[(size_t) b].load();
        curve.dyn[(size_t) b] = d; tabs.dyn[(size_t) b] = d;
        activity = std::max (activity, std::abs (d));
    }
    const float in = e.inPeak.exchange (0.0f), out = e.outPeak.exchange (0.0f);
    inMeter = std::max (in, inMeter * 0.86f);
    outMeter = std::max (out, outMeter * 0.86f);
    levels.in = inMeter; levels.out = outMeter; levels.repaint();
    eye.advance (activity, 1.0f / 30.0f);
    eye.repaint();
    curve.pullSpectrum();
    curve.repaint();
    tabs.repaint();

    const bool listening = (int) proc.apvts.getRawParameterValue ("listen")->load() == selected + 1;
    if (listenButton.getToggleState() != listening) listenButton.setToggleState (listening, juce::dontSendNotification);

    auto& rnd = juce::Random::getSystemRandom();
    const float before = flicker;
    flicker = (rnd.nextFloat() < 0.015f + juce::jlimit (0.0f, 0.25f, activity * 0.03f)) ? rnd.nextFloat() : flicker * 0.5f;
    if (std::abs (flicker - before) > 0.01f) repaint (110, 6, 370, 80);

    if (proc.presetVersion.load() != lastPresetVersion)
    {
        refreshPresetBox();
        noteLabel.setText (proc.bandNotes[selected], juce::dontSendNotification);
    }
}

//==============================================================================
void FreakEditor::refreshPresetBox()
{
    lastPresetVersion = proc.presetVersion.load();
    userFiles = FreakProcessor::getUserPresets();
    const auto current = proc.getCurrentPresetName();

    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.addSeparator();
    presetBox.addSectionHeading ("User Presets");
    if (userFiles.isEmpty()) { presetBox.addItem ("(none saved yet)", 999); presetBox.setItemEnabled (999, false); }
    for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), kUserIdBase + i);

    int sel = 0;
    for (int i = 0; i < userFiles.size(); ++i)
        if (userFiles[i].getFileNameWithoutExtension() == juce::File::createLegalFileName (current)) sel = kUserIdBase + i;
    if (sel == 0 && proc.getProgramName (proc.getCurrentProgram()) == current) sel = proc.getCurrentProgram() + 1;
    if (sel != 0) presetBox.setSelectedId (sel, juce::dontSendNotification);
    else presetBox.setText (current, juce::dontSendNotification);
    deleteButton.setEnabled (sel >= kUserIdBase);
}

void FreakEditor::stepPreset (int delta)
{
    std::vector<int> ids;
    for (int i = 0; i < proc.getNumPrograms(); ++i) ids.push_back (i + 1);
    for (int i = 0; i < userFiles.size(); ++i) ids.push_back (kUserIdBase + i);
    auto it = std::find (ids.begin(), ids.end(), presetBox.getSelectedId());
    const int pos = it == ids.end() ? 0 : (int) (it - ids.begin());
    const int n = (int) ids.size();
    presetBox.setSelectedId (ids[(size_t) (((pos + delta) % n + n) % n)], juce::sendNotificationSync);
}

void FreakEditor::showDialog (const juce::String& title, const juce::String& message, bool withTextField, const juce::String& initialText,
                                 juce::StringArray buttons, std::function<void (int, const juce::String&)> onResult)
{
    juce::Component::SafePointer<FreakEditor> safe (this);
    dialog = std::make_unique<FreakDialog> (title, message, withTextField, initialText, buttons,
        [safe, onResult] (int index, const juce::String& text)
        {
            juce::MessageManager::callAsync ([safe, onResult, index, text]
            {
                if (safe == nullptr) return;
                safe->dialog.reset();
                if (onResult) onResult (index, text);
            });
        });
    addAndMakeVisible (*dialog);
    dialog->setBounds (getLocalBounds());
}

void FreakEditor::showMessage (const juce::String& title, const juce::String& text)
{
    showDialog (title, text, false, {}, { "OK" }, {});
}

void FreakEditor::promptSave()
{
    const bool onUser = presetBox.getSelectedId() >= kUserIdBase;
    juce::Component::SafePointer<FreakEditor> safe (this);
    auto doSave = [safe] (const juce::String& name)
    {
        if (safe == nullptr) return;
        if (! safe->proc.saveUserPreset (name))
            safe->showMessage ("COULDN'T SAVE", "The Freak couldn't write the preset file to:\n" + FreakProcessor::getUserPresetFolder().getFullPathName());
        else safe->refreshPresetBox();
    };
    showDialog ("SAVE PRESET", "Name your preset:", true, onUser ? proc.getCurrentPresetName() : juce::String(), { "Save", "Cancel" },
        [safe, doSave] (int button, const juce::String& text)
        {
            if (button != 0 || safe == nullptr) return;
            const auto name = text.trim();
            if (name.isEmpty()) { safe->showMessage ("SAVE PRESET", "Please enter a name for the preset."); return; }
            for (int i = 0; i < safe->proc.getNumPrograms(); ++i)
                if (safe->proc.getProgramName (i).equalsIgnoreCase (name))
                {
                    safe->showMessage ("SAVE PRESET", "\"" + name + "\" is a factory preset name. Please choose another name.");
                    return;
                }
            if (FreakProcessor::fileForPresetName (name).existsAsFile())
            {
                safe->showDialog ("REPLACE PRESET?", "A preset named \"" + name + "\" already exists. Replace it with the current settings?",
                                  false, {}, { "Replace", "Cancel" },
                                  [doSave, name] (int b, const juce::String&) { if (b == 0) doSave (name); });
                return;
            }
            doSave (name);
        });
}

void FreakEditor::confirmDelete()
{
    const int id = presetBox.getSelectedId();
    if (id < kUserIdBase || id - kUserIdBase >= userFiles.size()) return;
    const auto file = userFiles[id - kUserIdBase];
    juce::Component::SafePointer<FreakEditor> safe (this);
    showDialog ("DELETE PRESET?", "Delete \"" + file.getFileNameWithoutExtension() + "\"? This can't be undone.", false, {},
                { "Delete", "Cancel" }, [safe, file] (int b, const juce::String&)
                {
                    if (b != 0 || safe == nullptr) return;
                    file.deleteFile();
                    safe->proc.setCurrentProgram (0);
                    safe->refreshPresetBox();
                });
}

//==============================================================================
FreakDialog::FreakDialog (const juce::String& t, const juce::String& m, bool withTextField, const juce::String& initialText,
                        juce::StringArray names, std::function<void (int, const juce::String&)> cb)
    : title (t), message (m), hasField (withTextField), onClose (std::move (cb))
{
    setWantsKeyboardFocus (true);
    if (hasField)
    {
        field.setText (initialText, false);
        field.setInputRestrictions (60);
        field.setSelectAllWhenFocused (true);
        field.setFont (tex::plainBold (15.0f));
        field.setIndents (8, 6);
        field.onReturnKey = [this] { finish (0); };
        field.onEscapeKey = [this] { finish ((int) buttons.size() - 1); };
        addAndMakeVisible (field);
    }
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (names[i]));
        b->onClick = [this, i] { finish (i); };
        addAndMakeVisible (b);
    }
}

void FreakDialog::parentHierarchyChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<FreakDialog> (this)]
        {
            if (sp == nullptr) return;
            if (sp->hasField) sp->field.grabKeyboardFocus(); else sp->grabKeyboardFocus();
        });
}

void FreakDialog::finish (int index)
{
    if (onClose == nullptr) return;
    auto cb = std::move (onClose);
    onClose = nullptr;
    cb (index, field.getText());
}

bool FreakDialog::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::returnKey) { finish (0); return true; }
    if (k == juce::KeyPress::escapeKey) { finish ((int) buttons.size() - 1); return true; }
    return true;
}

void FreakDialog::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (400, hasField ? 196 : 166);
    auto r = panel.reduced (22);
    r.removeFromTop (30);
    r.removeFromTop (hasField ? 22 : 50);
    if (hasField) { field.setBounds (r.removeFromTop (32)); r.removeFromTop (16); }
    auto row = r.removeFromBottom (28);
    for (int i = (int) buttons.size() - 1; i >= 0; --i) { buttons[i]->setBounds (row.removeFromRight (96)); row.removeFromRight (8); }
}

void FreakDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto p = panel.toFloat();
    g.setColour (juce::Colour (0xff1a1411)); g.fillRect (p);
    g.setGradientFill (juce::ColourGradient (pal::rustDeep.withAlpha (0.5f), p.getX(), p.getY(), juce::Colours::transparentBlack, p.getX(), p.getY() + p.getHeight() * 0.6f, false));
    g.fillRect (p);
    g.setColour (pal::rust); g.drawRect (p, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRect (p.reduced (2.0f), 1.0f);
    for (auto pt : { juce::Point<float> (p.getX() + 8, p.getY() + 8), { p.getRight() - 8, p.getY() + 8 }, { p.getX() + 8, p.getBottom() - 8 }, { p.getRight() - 8, p.getBottom() - 8 } })
    {
        g.setColour (juce::Colour (0xff6a5a4c)); g.fillEllipse (pt.x - 3, pt.y - 3, 6, 6);
        g.setColour (pal::rust.withAlpha (0.6f)); g.drawEllipse (pt.x - 3, pt.y - 3, 6, 6, 1.0f);
    }
    auto r = panel.reduced (22);
    g.setColour (pal::signalHot); g.setFont (tex::stencil (19.0f).withExtraKerningFactor (0.1f));
    g.drawText (title, r.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (pal::paint); g.setFont (tex::plainBold (13.0f));
    g.drawFittedText (message, r.removeFromTop (hasField ? 22 : 50), juce::Justification::topLeft, 3);
}

