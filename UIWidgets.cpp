#include "UIWidgets.h"

// ── Clipboard helpers ─────────────────────────────────────────────────────────
// Retrieves text content from system clipboard (platform-specific)
#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__)
#include <cstdio>
#include <array>
#endif


static std::string getClipboard() {
#ifdef __APPLE__
    PasteboardRef pb;
    if (PasteboardCreate(kPasteboardClipboard, &pb) != noErr) return "";
    PasteboardSynchronize(pb);
    ItemCount count = 0;
    PasteboardGetItemCount(pb, &count);
    std::string result;
    for (UInt32 i = 1; i <= count; ++i) {
        PasteboardItemID itemID;
        if (PasteboardGetItemIdentifier(pb, i, &itemID) != noErr) continue;
        CFDataRef data = nullptr;
        if (PasteboardCopyItemFlavorData(pb, itemID,
            CFSTR("public.utf8-plain-text"), &data) == noErr && data) {
            result = std::string(
                reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                CFDataGetLength(data));
            CFRelease(data);
            break;
        }
    }
    CFRelease(pb);
    return result;

#elif defined(__linux__)
    // Try xclip first, fall back to xsel
    FILE* pipe = popen("xclip -selection clipboard -out 2>/dev/null", "r");
    if (!pipe) {
        pipe = popen("xsel --clipboard --output 2>/dev/null", "r");
        if (!pipe) return "";
    }
    std::string result;
    std::array<char, 256> buf;
    while (fgets(buf.data(), buf.size(), pipe))
        result += buf.data();
    pclose(pipe);
    return result;
#else
    return "";
#endif
}

static void setClipboard(const std::string& text) {
#ifdef __APPLE__
    PasteboardRef pb;
    if (PasteboardCreate(kPasteboardClipboard, &pb) != noErr) return;
    PasteboardClear(pb);
    CFDataRef data = CFDataCreate(kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(text.c_str()), text.size());
    if (data) {
        PasteboardPutItemFlavor(pb, (PasteboardItemID)1,
            CFSTR("public.utf8-plain-text"), data, 0);
        CFRelease(data);
    }
    CFRelease(pb);

#elif defined(__linux__)
    // Try xclip first, fall back to xsel
    FILE* pipe = popen("xclip -selection clipboard -in 2>/dev/null", "w");
    if (!pipe) {
        pipe = popen("xsel --clipboard --input 2>/dev/null", "w");
        if (!pipe) return;
    }
    fwrite(text.c_str(), 1, text.size(), pipe);
    pclose(pipe);
#endif
}

// ── PillDropdown ──────────────────────────────────────────────────────────────
// Handles option selection and dropdown toggling logic
// Handles scroll bounds and visible item window
// Renders dropdown header including arrow indicator
// Renders dropdown items with highlighting and scrolling
void PillDropdown::init(float x, float y, float width, float height,
                        const std::vector<std::string>& opts, int idx) {
    cx=x; cy=y; w=width; h=height; options=opts;
    index = std::clamp(idx, 0, (int)opts.size()-1);
}

bool PillDropdown::hitHeader(sf::Vector2f p) const {
    return pillHit(cx, cy, w, h, p);
}

bool PillDropdown::onRelease(sf::Vector2f p) {
    if (open) {
        float listTop = cy + h/2.f + 2.f;
        int vis = std::min((int)options.size(), MAX_VIS);
        for (int i = 0; i < vis; ++i) {
            int ri = i + scrollOffset;
            if (ri >= (int)options.size()) break;
            float iy = listTop + i * ITEM_H;
            if (p.x >= cx-w/2.f && p.x <= cx+w/2.f &&
                p.y >= iy && p.y <= iy+ITEM_H) {
                index=ri; open=false; scrollOffset=0; return true;
            }
        }
        open=false; scrollOffset=0; return false;
    }
    if (hitHeader(p)) { open=true; return true; }
    return false;
}

void PillDropdown::onScroll(sf::Vector2f p, float delta) {
    if (!open) return;
    float listTop = cy + h/2.f + 2.f;
    int vis = std::min((int)options.size(), MAX_VIS);
    if (p.x >= cx-w/2.f && p.x <= cx+w/2.f &&
        p.y >= listTop && p.y <= listTop + vis*ITEM_H) {
        scrollOffset -= (int)delta;
        int maxS = std::max(0,(int)options.size()-MAX_VIS);
        scrollOffset = std::clamp(scrollOffset, 0, maxS);
    }
}

void PillDropdown::drawHeader(sf::RenderWindow& win, const sf::Font& font) const {
    drawPill(win, cx, cy, w, h, sf::Color(225,225,223));

    if (!options.empty()) {
        sf::Text t(font, options[index], (unsigned)(h*0.40f));
        t.setFillColor(sf::Color(60,60,60));
        t.setOrigin(centeredOrigin(t));
        t.setPosition({cx - h*0.3f, cy});
        win.draw(t);
    }

    float bsz = h * 0.70f;
    float bx  = cx + w/2.f - h/2.f;
    sf::RectangleShape box({bsz, bsz});
    box.setOrigin({bsz/2.f, bsz/2.f});
    box.setPosition({bx, cy});
    box.setFillColor(sf::Color(195,195,193));
    win.draw(box);

    sf::Text ch(font, open?"^":"v", (unsigned)(h*0.34f));
    ch.setFillColor(sf::Color(70,70,70));
    ch.setOrigin(centeredOrigin(ch));
    ch.setPosition({bx, cy + (open?1.f:-1.f)});
    win.draw(ch);
}

void PillDropdown::drawList(sf::RenderWindow& win, const sf::Font& font) const {
    if (!open) return;
    float listTop = cy + h/2.f + 2.f;
    int vis = std::min((int)options.size(), MAX_VIS);
    for (int i = 0; i < vis; ++i) {
        int ri = i + scrollOffset;
        if (ri >= (int)options.size()) break;
        float iy = listTop + i * ITEM_H;
        sf::RectangleShape item({w, ITEM_H});
        item.setPosition({cx-w/2.f, iy});
        item.setFillColor(ri==index ? sf::Color(205,205,203) : sf::Color(230,230,228));
        item.setOutlineThickness(0.5f);
        item.setOutlineColor(sf::Color(200,200,198));
        win.draw(item);
        sf::Text lbl(font, options[ri], (unsigned)(ITEM_H*0.42f));
        lbl.setFillColor(sf::Color(40,40,40));
        lbl.setOrigin(centeredOrigin(lbl));
        lbl.setPosition({cx, iy+ITEM_H/2.f});
        win.draw(lbl);
    }
}

// ── PillTextField ─────────────────────────────────────────────────────────────
// Manages focus state and cursor blinking
// Handles double-click detection for select-all behavior
// Deletes selected text range safely
// Handles character input, including backspace/delete logic and length limiting
// Handles keyboard navigation and clipboard shortcuts (copy/paste/cut)
// Renders text, selection highlight, and blinking cursor
void PillTextField::init(float x, float y, float width, float height,
                         const std::string& initial, const std::string& ph) {
    cx=x; cy=y; w=width; h=height;
    text=initial; placeholder=ph;
    cursorPos = (int)initial.size();
    selStart = selEnd = cursorPos;
}

bool PillTextField::hit(sf::Vector2f p) const { return pillHit(cx,cy,w,h,p); }

void PillTextField::setFocus(bool f) {
    focused = f;
    if (f) blinkClock.restart();
    else   clearSelection();
}

void PillTextField::onClick() {
    // double-click within 400ms -> select all
    if (clickArmed && lastClickClock.getElapsedTime().asMilliseconds() < 400) {
        selectAll();
        clickArmed = false;
    } else {
        clickArmed = true;
        lastClickClock.restart();
        clearSelection();
    }
    blinkClock.restart();
}

void PillTextField::deleteSelection() {
    if (!hasSelection()) return;
    int lo = std::min(selStart, selEnd);
    int hi = std::max(selStart, selEnd);
    lo = std::clamp(lo, 0, (int)text.size());
    hi = std::clamp(hi, 0, (int)text.size());
    text.erase(lo, hi - lo);
    cursorPos = lo;
    selStart = selEnd = lo;
}

static constexpr int MAX_LEN = 300;

void PillTextField::onTextEntered(uint32_t u) {
    if (!focused) return;

    // always clamp cursorPos to valid range first
    cursorPos = std::clamp(cursorPos, 0, (int)text.size());

    if (u == 8) {  // backspace
        if (hasSelection()) {
            deleteSelection();
        } else if (cursorPos > 0) {
            text.erase(cursorPos - 1, 1);
            --cursorPos;
        }
    } else if (u == 127) {  // delete (forward)
        if (hasSelection()) {
            deleteSelection();
        } else if (cursorPos < (int)text.size()) {
            text.erase(cursorPos, 1);
        }
    } else if (u >= 32 && u < 127) {
        if ((int)text.size() >= MAX_LEN) return;  // at limit, ignore
        if (hasSelection()) deleteSelection();
        cursorPos = std::clamp(cursorPos, 0, (int)text.size());
        text.insert(cursorPos, 1, (char)u);
        ++cursorPos;
    }

    selStart = selEnd = cursorPos;
    blinkClock.restart();
}

void PillTextField::onKeyPressed(sf::Keyboard::Key key, bool modifier) {
    if (!focused) return;

    cursorPos = std::clamp(cursorPos, 0, (int)text.size());

    if (modifier && key == sf::Keyboard::Key::A) {
        selectAll();
    } else if (modifier && key == sf::Keyboard::Key::C) {
        if (hasSelection()) {
            int lo = std::min(selStart, selEnd);
            int hi = std::max(selStart, selEnd);
            setClipboard(text.substr(lo, hi - lo));
        } else {
            setClipboard(text);
        }
    } else if (modifier && key == sf::Keyboard::Key::X) {
        if (hasSelection()) {
            int lo = std::min(selStart, selEnd);
            int hi = std::max(selStart, selEnd);
            setClipboard(text.substr(lo, hi - lo));
            deleteSelection();
        }
    } else if (modifier && key == sf::Keyboard::Key::V) {
        std::string clip = getClipboard();
        clip.erase(std::remove_if(clip.begin(), clip.end(),
            [](char c){ return (unsigned char)c < 32 || (unsigned char)c >= 127; }),
            clip.end());
        if (hasSelection()) deleteSelection();
        cursorPos = std::clamp(cursorPos, 0, (int)text.size());
        // truncate paste so total length doesn't exceed MAX_LEN
        int room = MAX_LEN - (int)text.size();
        if (room <= 0) return;
        if ((int)clip.size() > room) clip = clip.substr(0, room);
        text.insert(cursorPos, clip);
        cursorPos += (int)clip.size();
        selStart = selEnd = cursorPos;
    } else if (key == sf::Keyboard::Key::Left && cursorPos > 0) {
        --cursorPos; clearSelection();
    } else if (key == sf::Keyboard::Key::Right && cursorPos < (int)text.size()) {
        ++cursorPos; clearSelection();
    } else if (key == sf::Keyboard::Key::Home) {
        cursorPos = 0; clearSelection();
    } else if (key == sf::Keyboard::Key::End) {
        cursorPos = (int)text.size(); clearSelection();
    }
    else if (key==sf::Keyboard::Key::Left  && cursorPos>0)                --cursorPos;
    else if (key==sf::Keyboard::Key::Right && cursorPos<(int)text.size()) ++cursorPos;
    else if (key==sf::Keyboard::Key::Home)  cursorPos=0;
    else if (key==sf::Keyboard::Key::End)   cursorPos=(int)text.size();
}

void PillTextField::draw(sf::RenderWindow& win, const sf::Font& font, sf::Color accent) {
    // clamp cursor/selection to valid range defensively
    cursorPos = std::clamp(cursorPos, 0, (int)text.size());
    selStart  = std::clamp(selStart,  0, (int)text.size());
    selEnd    = std::clamp(selEnd,    0, (int)text.size());

    auto pill = makePill(cx, cy, w, h);
    pill.setFillColor(sf::Color(225,225,223));
    if (focused) { pill.setOutlineThickness(2.f); pill.setOutlineColor(accent); }
    win.draw(pill);

    bool empty = text.empty();
    std::string shown = empty ? placeholder : text;

    sf::Text display(font, shown, (unsigned)(h*0.40f));
    display.setFillColor(empty ? sf::Color(170,170,170) : sf::Color(50,50,50));

    int shownOffset = 0;
    float maxW = w - h;
    while (!shown.empty() && display.getLocalBounds().size.x > maxW) {
        shown = shown.substr(1); ++shownOffset; display.setString(shown);
    }

    float textX = cx - w/2.f + h/2.f;
    auto db = display.getLocalBounds();
    display.setOrigin({db.position.x, db.position.y + db.size.y/2.f});
    display.setPosition({textX, cy});

    // draw selection highlight
    if (!empty && focused && hasSelection()) {
        int lo = std::clamp(std::min(selStart, selEnd) - shownOffset, 0, (int)shown.size());
        int hi = std::clamp(std::max(selStart, selEnd) - shownOffset, 0, (int)shown.size());
        if (lo < hi) {
            sf::Text mLo(font, shown.substr(0, lo), (unsigned)(h*0.40f));
            sf::Text mHi(font, shown.substr(0, hi), (unsigned)(h*0.40f));
            float x0 = textX + mLo.getLocalBounds().size.x;
            float x1 = textX + mHi.getLocalBounds().size.x;
            sf::RectangleShape sel({x1-x0, h*0.60f});
            sel.setPosition({x0, cy - h*0.30f});
            sel.setFillColor(sf::Color(accent.r, accent.g, accent.b, 80));
            win.draw(sel);
        }
    }

    win.draw(display);

    // cursor blink
    if (focused && !hasSelection()) {
        int ms = (int)blinkClock.getElapsedTime().asMilliseconds();
        if (ms % 1000 < 600) {
            int vc = std::max(0, cursorPos - shownOffset);
            std::string upto = shown.substr(0, std::min(vc, (int)shown.size()));
            sf::Text meas(font, upto, (unsigned)(h*0.40f));
            float curX = textX + meas.getLocalBounds().size.x + 2.f;
            sf::RectangleShape cur({2.f, h*0.55f});
            cur.setPosition({curX, cy - h*0.275f});
            cur.setFillColor(sf::Color(80,80,80));
            win.draw(cur);
        }
    }
}

// ── Slider ────────────────────────────────────────────────────────────────────
// Converts slider value to on-screen thumb position
// Checks expanded interaction zone for easier dragging
// Maps mouse X position to slider value (0–100)
// Draws track, filled portion, and draggable thumb

void Slider::init(float x, float y, float width, int v) {
    cx=x; cy=y; w=width; value=std::clamp(v,0,100);
}

float Slider::thumbX() const { return cx + w*value/100.f; }

bool Slider::hitArea(sf::Vector2f p) const {
    return p.x>=cx-14.f && p.x<=cx+w+14.f && std::abs(p.y-cy)<=16.f;
}

void Slider::applyX(float px) {
    value=(int)std::round(std::clamp((px-cx)/w*100.f,0.f,100.f));
}

void Slider::draw(sf::RenderWindow& win) const {
    sf::RectangleShape track({w,4.f});
    track.setPosition({cx,cy-2.f});
    track.setFillColor(sf::Color(195,195,193));
    win.draw(track);

    sf::RectangleShape fill({w*value/100.f,4.f});
    fill.setPosition({cx,cy-2.f});
    fill.setFillColor(sf::Color(100,100,100));
    win.draw(fill);

    sf::CircleShape thumb(10.f);
    thumb.setOrigin({10.f,10.f});
    thumb.setPosition({thumbX(),cy});
    thumb.setFillColor(sf::Color(80,80,80));
    win.draw(thumb);
}

// ── CircleButton ──────────────────────────────────────────────────────────────
// Checks radial distance for hit detection
// Draws button with hover feedback and icon
bool CircleButton::hit(sf::Vector2f p) const {
    float dx=p.x-cx, dy=p.y-cy; return dx*dx+dy*dy<=r*r;
}

void CircleButton::draw(sf::RenderWindow& win, const sf::Font& font,
                        sf::Vector2f mouse) const {
    sf::CircleShape c(r);
    c.setOrigin({r,r}); c.setPosition({cx,cy});
    c.setFillColor(hit(mouse) ? sf::Color(200,200,198) : sf::Color(218,218,216));
    win.draw(c);

    sf::Text arrow(font, "<", (unsigned)(r*1.1f));
    arrow.setFillColor(sf::Color(70,70,70));
    arrow.setOrigin(centeredOrigin(arrow));
    arrow.setPosition({cx, cy-1.f});
    win.draw(arrow);
}