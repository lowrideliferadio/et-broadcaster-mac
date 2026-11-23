#include "DeckView.h"
#include <cmath>

DeckView::DeckView(DeckChannel& c, juce::String n, juce::Colour a) : ch(c), name(std::move(n)), accent(a) {
    for (auto* b : { &load, &play, &kill }) addAndMakeVisible(b);
    load.onClick = [this] { if (onLoad) onLoad(); };
    play.onClick = [this] { ch.motor = ! ch.motor.load(); };
    kill.setClickingTogglesState(true);
    kill.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe0213f));
    kill.onClick = [this] { ch.killed = kill.getToggleState(); };
    pitch.setSliderStyle(juce::Slider::LinearVertical); pitch.setRange(-8.0, 8.0, 0.01); pitch.setValue(0.0);
    pitch.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18); pitch.setTextValueSuffix(" %");
    pitch.setDoubleClickReturnValue(true, 0.0);
    pitch.onValueChange = [this] { ch.pitch = (float) (1.0 + pitch.getValue() / 100.0); };
    addAndMakeVisible(pitch);
    startTimerHz(60);
}

void DeckView::resized() {
    auto r = getLocalBounds().reduced(10); auto top = r.removeFromTop(26); r.removeFromTop(6);
    auto bottom = r.removeFromBottom(34); r.removeFromBottom(6);
    pitch.setBounds(r.removeFromRight(64)); r.removeFromRight(8);
    const float side = (float) juce::jmin(r.getWidth(), r.getHeight());            // square → perfect circle
    platter = juce::Rectangle<float>(side, side).withCentre(r.toFloat().getCentre());
    centre = platter.getCentre();
    juce::FlexBox fb; fb.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    for (auto* b : { &load, &play, &kill }) fb.items.add(juce::FlexItem(*b).withFlex(1).withMargin({ 0, 3, 0, 3 }));
    fb.performLayout(bottom); juce::ignoreUnused(top);
}

void DeckView::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26272d), 0, 0, juce::Colour(0xff111215), 0, b.getHeight(), false));
    g.fillRoundedRectangle(b.reduced(1), 10);
    g.setColour(juce::Colours::white.withAlpha(0.08f)); g.drawRoundedRectangle(b.reduced(1.5f), 10, 1);
    g.setColour(accent); g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("DECK " + name, 14, 8, 90, 22, juce::Justification::left);
    g.setColour(juce::Colours::white.withAlpha(0.85f)); g.setFont(juce::FontOptions(14.0f));
    g.drawText(ch.title, 100, 8, getWidth() - 114, 22, juce::Justification::right, true);
    // platter: perfect circle — strobe dots ring, vinyl, grooves, rotating label
    auto p = platter; const float R = p.getWidth() * 0.5f;
    g.setColour(juce::Colour(0xff0b0b0d)); g.fillEllipse(p);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff5a5d66), p.getX(), p.getY(), juce::Colour(0xff1b1c20), p.getRight(), p.getBottom(), false));
    g.drawEllipse(p.reduced(1.5f), 3.0f);
    const double ang = ch.angleDeg.load() * juce::MathConstants<double>::pi / 180.0;
    for (int i = 0; i < 72; ++i) { const double a = ang + i * juce::MathConstants<double>::twoPi / 72;
        g.setColour(juce::Colours::white.withAlpha(0.35f)); g.fillEllipse(centre.x + (float) std::cos(a) * (R - 7) - 1.2f, centre.y + (float) std::sin(a) * (R - 7) - 1.2f, 2.4f, 2.4f); }
    auto vinyl = p.reduced(R * 0.08f); g.setColour(juce::Colour(0xff09090b)); g.fillEllipse(vinyl);
    for (float k = 0.36f; k < 0.92f; k += 0.035f) { g.setColour(juce::Colours::white.withAlpha(0.035f)); g.drawEllipse(juce::Rectangle<float>(R * 2 * k, R * 2 * k).withCentre(centre), 1.0f); }
    // specular sheen (fixed — light does not rotate with the record)
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.10f), centre.x - R * 0.6f, centre.y - R * 0.6f, juce::Colours::transparentWhite, centre.x, centre.y, true));
    g.fillEllipse(vinyl);
    auto label = juce::Rectangle<float>(R * 0.66f, R * 0.66f).withCentre(centre);
    g.setColour(accent.darker(0.3f)); g.fillEllipse(label);
    juce::Path mark; mark.addRectangle(-2.0f, -R * 0.33f, 4.0f, R * 0.14f);
    g.setColour(juce::Colours::white.withAlpha(0.9f)); g.fillPath(mark, juce::AffineTransform::rotation((float) ang).translated(centre));
    g.setColour(juce::Colour(0xffc9ccd3)); g.fillEllipse(juce::Rectangle<float>(7, 7).withCentre(centre));
    if (ch.killed) { g.setColour(juce::Colour(0x99e0213f)); g.fillEllipse(vinyl); g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(22.0f, juce::Font::bold)); g.drawText("KILLED", vinyl, juce::Justification::centred); }
    const double len = ch.lengthFrames.load(), pos = ch.position.load(), rate = ch.trackRate.load();
    if (len > 0) { const double left = juce::jmax(0.0, (len - pos) / rate);
        g.setColour(juce::Colour(0xffffb84a)); g.setFont(juce::FontOptions(14.0f));
        g.drawText(juce::String::formatted("-%d:%02d", (int) left / 60, (int) left % 60), getLocalBounds().removeFromBottom(46).removeFromLeft(80), juce::Justification::centredLeft); }
}

void DeckView::mouseDown(const juce::MouseEvent& e) {
    if (e.position.getDistanceFrom(centre) > platter.getWidth() * 0.5f) return;
    grabbing = true; ch.touch = true; lastAngle = std::atan2(e.position.y - centre.y, e.position.x - centre.x);
}
void DeckView::mouseDrag(const juce::MouseEvent& e) {
    if (! grabbing) return;
    const double a = std::atan2(e.position.y - centre.y, e.position.x - centre.x);
    double d = a - lastAngle; if (d > juce::MathConstants<double>::pi) d -= juce::MathConstants<double>::twoPi; if (d < -juce::MathConstants<double>::pi) d += juce::MathConstants<double>::twoPi;
    lastAngle = a; ch.addRevs(d / juce::MathConstants<double>::twoPi);
}
void DeckView::mouseUp(const juce::MouseEvent&) { if (grabbing) { grabbing = false; ch.touch = false; } }
