// A light Qt widget style that follows the Memrium shell's look.
//
// The shell writes ~/.config/memrium/appearance.toml (light or dark, the accent, how round and how deep the controls
// are); this style reads it, builds the palette from it and draws the common controls itself. Everything it does not
// draw is Fusion's, so nothing is missing. The file is watched: a program follows a theme switch while it is open.
// Use: QT_STYLE_OVERRIDE=chebrium QT_PLUGIN_PATH=<dir holding styles/libchebrium.so>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QStyleHints>
#include <QEasingCurve>
#include <QHash>
#include <QComboBox>
#include <QListView>
#include <QSpinBox>
#include <QTreeView>
#include <QToolBar>
#include <QToolButton>
#include <QHeaderView>
#include <QStyleOptionHeader>
#include <QStyleOptionProgressBar>
#include <QTabBar>
#include <QTabWidget>
#include <QStyleOptionTab>
#include <QStyleOptionSpinBox>
#include <QMenu>
#include <QItemSelectionModel>
#include <QVariantAnimation>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QScrollBar>
#include <QStyleOption>
#include <QStylePlugin>
#include <QWidget>
#include <cmath>

namespace {

struct Look {
    bool dark = false;
    QColor accent{10, 132, 255};
    qreal round = 1.0;  // 0 nearly square .. 1 pills
    qreal depth = 1.0;  // 0 flat .. 1.5 keys that stand out
    bool flat = false;
    qreal popupOpacity = 0.0;  // 0: like the windows
    bool slide = true;
    qreal opacity = 1.0;  // the theme's window opacity: below 1 the shell frosts what is behind a popup
};

QString appearancePath() {
    QByteArray env = qgetenv("CHEBRIUM_APPEARANCE");
    if (!env.isEmpty()) return QString::fromLocal8Bit(env);
    QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (base.isEmpty()) base = QDir::homePath() + "/.config";
    QString path = base + "/memrium-de/appearance.toml";
    // in a Flatpak sandbox XDG_CONFIG_HOME is the program's own; the shell's file is shared at the real ~/.config
    if (!QFile::exists(path) && QFile::exists(QDir::homePath() + "/.config/memrium/appearance.toml")) path = QDir::homePath() + "/.config/memrium/appearance.toml";
    return path;
}

// How the glass of popups is painted. 1 (or 0 for the offsets) is the look the style ships with; ~/.config/memrium/glass.toml
// (flat `dark_outline = 1.2` / `light_glow = 0.5` lines, written by chebrium-tweak) changes it while programs run.
struct Tune {
    qreal outline = 1, hairline = 1, inner = 1, band = 1, glow = 1, sheen = 1;  // strength of each part of the edge and the light
    qreal width = 1;                                                           // thickness of the bright outline, px
    qreal top = 0, bottom = 0, fill = 0;                                       // lightness of the body: top, bottom, all (-50 .. 50 %)
    qreal tint = 0;                                                            // how much of the accent colours the glass, 0 .. 1
    qreal radius = 0;                                                          // added to the corner radius, px
};

// what the dark glass is when glass.toml says nothing (picked with chebrium-tweak)
Tune darkDefault() {
    Tune t;
    t.outline = 0.13; t.width = 3; t.hairline = 0.29; t.inner = 0.24; t.band = 1.53; t.glow = 1.36; t.sheen = 2.0;
    t.fill = -8; t.top = 7; t.bottom = -0.4; t.tint = 0; t.radius = -3.75;
    return t;
}

QString glassPath() { return QFileInfo(appearancePath()).absolutePath() + "/glass.toml"; }

void readGlass(Tune &dark, Tune &light) {
    QFile f(glassPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        int eq = line.indexOf('=');
        if (line.startsWith('#') || eq < 0) continue;
        QString k = line.left(eq).trimmed();
        const qreal v = line.mid(eq + 1).trimmed().toDouble();
        Tune *t = k.startsWith("dark_") ? &dark : k.startsWith("light_") ? &light : nullptr;
        if (!t) continue;
        k = k.mid(k.indexOf('_') + 1);
        if (k == "outline") t->outline = v; else if (k == "hairline") t->hairline = v; else if (k == "inner") t->inner = v;
        else if (k == "band") t->band = v; else if (k == "glow") t->glow = v; else if (k == "sheen") t->sheen = v;
        else if (k == "width") t->width = v; else if (k == "top") t->top = v; else if (k == "bottom") t->bottom = v;
        else if (k == "fill") t->fill = v; else if (k == "tint") t->tint = v; else if (k == "radius") t->radius = v;
    }
}

// the file is flat `key = value` lines; anything missing keeps its default
void readFile(QFile &f, Look &l);

Look readLook() {
    Look l;
    QFile f(appearancePath());
    if (f.open(QIODevice::ReadOnly)) readFile(f, l);
    // the shell's rule for this one program (app-rules.toml) comes through the environment when it starts the program
    QByteArray dark = qgetenv("CHEBRIUM_DARK"), accent = qgetenv("CHEBRIUM_ACCENT");
    if (!dark.isEmpty()) l.dark = dark == "1";
    if (!accent.isEmpty()) {
        QList<QByteArray> c = accent.split(',');
        if (c.size() >= 3) l.accent = QColor::fromRgbF(c[0].toDouble(), c[1].toDouble(), c[2].toDouble());
    }
    return l;
}

void readFile(QFile &f, Look &l) {
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        int eq = line.indexOf('=');
        if (line.startsWith('#') || eq < 0) continue;
        QString k = line.left(eq).trimmed(), v = line.mid(eq + 1).trimmed();
        if (k == "dark") l.dark = v == "true";
        else if (k == "accent") {
            QStringList c = v.mid(1, v.size() - 2).split(',');
            if (c.size() >= 3) l.accent = QColor::fromRgbF(c[0].toDouble(), c[1].toDouble(), c[2].toDouble());
        } else if (k == "controls_round") l.round = qBound(0.0, v.toDouble(), 1.0);
        else if (k == "controls_depth") l.depth = qBound(0.0, v.toDouble(), 1.5);
        else if (k == "controls") l.flat = v.contains("flat");
        else if (k == "opacity") l.opacity = qBound(0.0, v.toDouble(), 1.0);
        else if (k == "popup_opacity") l.popupOpacity = qBound(0.0, v.toDouble(), 1.0);
        else if (k == "slide") l.slide = v == "true";
    }
}

QColor mix(const QColor &a, const QColor &b, qreal t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QPalette makePalette(const Look &l) {
    QColor window, base, button, text, mid;
    if (l.dark) {
        window = {33, 33, 33}; base = {27, 27, 27}; button = {48, 48, 48}; text = {238, 238, 238}; mid = {82, 82, 82};
    } else {
        window = {247, 249, 255}; base = {255, 255, 255}; button = {229, 232, 240}; text = {28, 30, 36}; mid = {190, 196, 210};
    }
    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, mix(base, text, 0.04));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, l.accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, l.dark ? QColor(60, 60, 60) : QColor(255, 255, 255));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, mix(base, text, 0.45));
    p.setColor(QPalette::Link, mix(l.accent, text, l.dark ? 0.0 : 0.15));
    p.setColor(QPalette::Light, mix(window, Qt::white, 0.5));
    p.setColor(QPalette::Midlight, mix(window, mid, 0.4));
    p.setColor(QPalette::Mid, mid);
    p.setColor(QPalette::Dark, mix(window, Qt::black, 0.35));
    p.setColor(QPalette::Shadow, Qt::black);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, mix(window, text, 0.4));
    p.setColor(QPalette::Disabled, QPalette::Highlight, mix(window, l.accent, 0.5));
    // a window that is not in front keeps its chosen rows pale, with ink on them (white would vanish on the pale colour)
    p.setColor(QPalette::Inactive, QPalette::Highlight, mix(window, l.accent, 0.45));
    p.setColor(QPalette::Inactive, QPalette::HighlightedText, text);
    return p;
}

}  // namespace

class ChebriumStyle : public QProxyStyle {
public:
    ChebriumStyle() : QProxyStyle("Fusion") {
        load();
        // an editor that saves by renaming drops the file's watch: watch the directory too and add the file again
        auto *w = new QFileSystemWatcher(this);
        QString path = appearancePath();
        w->addPath(QFileInfo(path).absolutePath());
        if (QFile::exists(path)) w->addPath(path);
        const QString gpath = glassPath();
        if (QFile::exists(gpath)) w->addPath(gpath);
        auto reload = [this, w, path, gpath] {
            if (QFile::exists(path) && !w->files().contains(path)) w->addPath(path);
            if (QFile::exists(gpath) && !w->files().contains(gpath)) w->addPath(gpath);
            load();
            if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
                app->setPalette(pal);
                applyScheme();
                for (QWidget *x : QApplication::allWidgets()) x->update();
            }
        };
        connect(w, &QFileSystemWatcher::fileChanged, this, reload);
        connect(w, &QFileSystemWatcher::directoryChanged, this, reload);
    }

    QPalette standardPalette() const override { return pal; }
    void polish(QPalette &p) override { p = pal; }
    void polish(QApplication *app) override {
        QProxyStyle::polish(app);
        app->setPalette(pal);
        applyScheme();
        // KDE programs set their own colour scheme (kdeglobals) after the style: take the palette back when they do
        app->installEventFilter(this);
        // the shell's own font, Inter, unless the program's rule (CHEBRIUM_FONT) names another one
        QByteArray font = qgetenv("CHEBRIUM_FONT");
        if (font.isEmpty() && loadInter()) font = "Inter";
        if (!font.isEmpty()) { QFont f = app->font(); f.setFamily(QString::fromUtf8(font)); app->setFont(f); }
    }
    // The two Inter faces compiled into the plugin go into Qt's font database; true when Inter is there to use
    static bool loadInter() {
        static const bool ok = [] {
            bool any = false;
            for (const char *face : {":/chebrium/Inter-Regular.otf", ":/chebrium/Inter-SemiBold.otf"}) {
                QFile f(QString::fromLatin1(face));
                if (f.open(QIODevice::ReadOnly) && QFontDatabase::addApplicationFontFromData(f.readAll()) >= 0) any = true;
            }
            return any;
        }();
        return ok;
    }
    void polish(QWidget *w) override {
        QProxyStyle::polish(w);
        if (qobject_cast<QScrollBar *>(w)) w->setAttribute(Qt::WA_Hover);
        // the buttons of a tool bar share one highlight that glides from the button the pointer left to the one it entered
        if (qobject_cast<QToolButton *>(w) && qobject_cast<QToolBar *>(w->parentWidget())) w->installEventFilter(this);
        if (qobject_cast<QMenu *>(w)) {
            // the size of text in the shell's own menus
            QFont f = w->font();
            f.setPointSizeF(13.5 * 72.0 / 96.0);
            w->setFont(f);
        }
        if (roundPopup(w)) {
            w->setAttribute(Qt::WA_TranslucentBackground);
            if (w->inherits("QComboBoxPrivateContainer")) {
                w->setAutoFillBackground(false);
                w->setAttribute(Qt::WA_NoSystemBackground);
            }
        }
        if (auto *lv = qobject_cast<QListView *>(w); lv && slidable(lv)) attachSlide(lv);
    }
    void unpolish(QWidget *w) override {
        if (qobject_cast<QToolButton *>(w)) w->removeEventFilter(this);
        if (qobject_cast<QScrollBar *>(w)) w->setAttribute(Qt::WA_Hover, false);
        if (roundPopup(w)) w->setAttribute(Qt::WA_TranslucentBackground, false);
        if (auto *lv = qobject_cast<QListView *>(w); lv && underlays.contains(lv)) detachSlide(lv);
        QProxyStyle::unpolish(w);
    }

    int pixelMetric(PixelMetric m, const QStyleOption *o, const QWidget *w) const override {
        switch (m) {
        case PM_ScrollBarExtent: return 12;
        case PM_MenuPanelWidth: return 6;
        // room between the edge of the highlight and the text of a row
        case PM_FocusFrameHMargin:
            if (const auto *lv = qobject_cast<const QListView *>(w); lv && underlays.contains(lv)) return 9;
            return QProxyStyle::pixelMetric(m, o, w);
        case PM_ScrollBarSliderMin: return 28;
        case PM_IndicatorWidth:
        case PM_IndicatorHeight:
        case PM_ExclusiveIndicatorWidth:
        case PM_ExclusiveIndicatorHeight: return 16;
        case PM_SliderThickness:
        case PM_SliderLength: return 18;
        default: return QProxyStyle::pixelMetric(m, o, w);
        }
    }

    int styleHint(StyleHint h, const QStyleOption *o, const QWidget *w, QStyleHintReturn *r) const override {
        // the popup of a combo box is a list under the box (not a menu over it): the list's highlight can slide
        if (h == SH_ComboBox_Popup) return 0;
        // the chosen row of a tree is coloured all across, not only behind its text
        if (h == SH_ItemView_ShowDecorationSelected) return 1;
        if (h == SH_Table_GridLineColor) return mix(pal.color(QPalette::Base), pal.color(QPalette::Text), 0.08).rgba();
        return QProxyStyle::styleHint(h, o, w, r);
    }

    QSize sizeFromContents(ContentsType t, const QStyleOption *o, const QSize &sz, const QWidget *w) const override {
        QSize s = QProxyStyle::sizeFromContents(t, o, sz, w);
        if (t == CT_ItemViewItem && w && w->inherits("QComboBoxListView")) return QSize(s.width() + 20, qMax(s.height() + 14, 34));
        if (t == CT_MenuItem) {
            const auto *m = qstyleoption_cast<const QStyleOptionMenuItem *>(o);
            if (m && m->menuItemType == QStyleOptionMenuItem::Separator) return QSize(s.width(), 11);
            // roomy rows, as the shell's own menus have
            return QSize(s.width() + 34, 30);
        }
        return s;
    }

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *o, SubControl sc, const QWidget *w) const override {
        if (cc != CC_ScrollBar) return QProxyStyle::subControlRect(cc, o, sc, w);
        // no arrow buttons: the groove is the whole bar
        auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(o);
        if (!sb) return QProxyStyle::subControlRect(cc, o, sc, w);
        const QRect r = o->rect;
        const bool h = sb->orientation == Qt::Horizontal;
        const int len = h ? r.width() : r.height();
        const int range = sb->maximum - sb->minimum;
        int sl = range <= 0 ? len : int(qint64(len) * sb->pageStep / (range + sb->pageStep));
        sl = qBound(qMin(pixelMetric(PM_ScrollBarSliderMin, o, w), len), sl, len);
        const int pos = sliderPositionFromValue(sb->minimum, sb->maximum, sb->sliderPosition, len - sl, sb->upsideDown);
        auto part = [&](int from, int size) { return h ? QRect(r.x() + from, r.y(), size, r.height()) : QRect(r.x(), r.y() + from, r.width(), size); };
        QRect out;
        switch (sc) {
        case SC_ScrollBarGroove: out = r; break;
        case SC_ScrollBarSlider: out = part(pos, sl); break;
        case SC_ScrollBarSubPage: out = part(0, pos); break;
        case SC_ScrollBarAddPage: out = part(pos + sl, len - pos - sl); break;
        default: return QRect();
        }
        return h ? visualRect(sb->direction, r, out) : out;
    }

    void drawPrimitive(PrimitiveElement e, const QStyleOption *o, QPainter *p, const QWidget *w) const override {
        const bool en = o->state & State_Enabled;
        switch (e) {
        case PE_PanelButtonCommand: {
            const auto *b = qstyleoption_cast<const QStyleOptionButton *>(o);
            const bool def = b && (b->features & QStyleOptionButton::DefaultButton);
            key(p, o, o->rect, def ? look.accent : (o->state & State_On) ? mix(pal.color(QPalette::Button), look.accent, 0.35) : QColor());
            return;
        }
        case PE_PanelButtonTool:
            // a button of a tool bar is lit by the bar's gliding highlight; only a pressed or chosen one has its own key
            if (w && qobject_cast<QToolBar *>(w->parentWidget()) && !(o->state & (State_Sunken | State_On))) return;
            if (o->state & (State_Sunken | State_On | State_MouseOver)) key(p, o, o->rect, (o->state & State_On) ? mix(pal.color(QPalette::Button), look.accent, 0.35) : QColor());
            return;
        case PE_FrameFocusRect: return;
        // the row or cell under the pointer or chosen: a soft pill (a list with the sliding highlight paints its own)
        case PE_PanelItemViewRow:
        case PE_PanelItemViewItem: {
            const auto *vo = qstyleoption_cast<const QStyleOptionViewItem *>(o);
            if (!vo) break;
            const bool selected = vo->state & State_Selected, hover = (vo->state & State_MouseOver) && (vo->state & State_Enabled);
            const bool row = e == PE_PanelItemViewRow;
            // a view with alternating rows: its odd rows are a faint band (drawn with the row, not with every cell)
            if (row && (vo->features & QStyleOptionViewItem::Alternate)) {
                p->fillRect(vo->rect, pal.color(QPalette::AlternateBase));
            }
            if (!selected && !hover) return;
            // a table cell or a list item fills its rect; a tree row is one band across all its columns, drawn once with the row
            // (the cells of a tree draw nothing of their own)
            QRect box = vo->rect;
            // the band of a tree is made of the branch area (its round left end) and the cells (the last one has the round right end)
            bool roundL = true, roundR = true;
            if (const auto *tv = qobject_cast<const QTreeView *>(w)) {
                roundL = row;
                roundR = false;
                if (!row) {
                    const QHeaderView *hd = tv->header();
                    int last = -1;
                    for (int v = hd->count() - 1; v >= 0 && last < 0; v--) if (!hd->isSectionHidden(hd->logicalIndex(v))) last = hd->logicalIndex(v);
                    roundR = vo->index.column() == last;
                }
            }
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            if (selected) {
                QColor c = look.accent;
                if (!(vo->state & State_Active)) c = mix(pal.color(QPalette::Window), c, 0.55);
                p->setBrush(c);
            } else {
                p->setBrush(mix(pal.color(QPalette::Base), pal.color(QPalette::Text), 0.07));
            }
            const qreal rr = qMin<qreal>(8 + 2 * look.round, vo->rect.height() / 2.0);
            // a square end is a round one pushed out of the cell, which the clip cuts off
            p->setClipRect(box, Qt::IntersectClip);
            p->drawRoundedRect(QRectF(box).adjusted(roundL ? 2 : -rr - 2, 1, roundR ? -2 : rr + 2, -1), rr, rr);
            p->restore();
            return;
        }
        case PE_PanelTipLabel:
        case PE_PanelMenu: {
            // a popup: a round card in the colour of popovers, an edge, no square corners behind it
            const QRectF r = QRectF(o->rect).adjusted(.5, .5, -.5, -.5);
            const qreal rr = (e == PE_PanelTipLabel ? 10 : 12 + 6 * look.round) + tune().radius;
            {
                QColor tip = pal.color(QPalette::ToolTipBase);
                tip.setAlphaF(popupAlpha());
                glassCard(p, r, rr, e == PE_PanelTipLabel ? tip : popupFill());
            }
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            if (const auto *menu = qobject_cast<const QMenu *>(w); menu && e == PE_PanelMenu) {
                // Qt repaints only the rows an active item left and entered, but the highlight is the card's and it moves over the
                // rows in between (and stays where it was when the active item goes away): a paint that does not cover the whole menu
                // is followed by one that does, or a stripe of the earlier highlight stays on screen
                if (!p->clipBoundingRect().contains(QRectF(menu->rect()).adjusted(1, 1, -1, -1)))
                    QMetaObject::invokeMethod(const_cast<QMenu *>(menu), qOverload<>(&QWidget::update), Qt::QueuedConnection);
                if (const Slide *sl = slideOf(menu); sl->valid) {
                    p->setPen(Qt::NoPen);
                    p->setBrush(look.accent);
                    const qreal hr = 8 + 4 * look.round;
                    p->drawRoundedRect(sl->cur.adjusted(0, 1, 0, -1), hr, hr);
                }
            }
            p->restore();
            return;
        }
        case PE_FrameMenu:
            // the popup of a combo box is a frame of its own, not a QMenu: it is the card
            if (w && w->inherits("QComboBoxPrivateContainer")) {
                const QRectF r = QRectF(o->rect).adjusted(.5, .5, -.5, -.5);
                const qreal rr = 12 + 6 * look.round + tune().radius;
                glassCard(p, r, rr, popupFill());
            }
            return;
        case PE_Frame: {
            if (w && w->inherits("QComboBoxPrivateContainer")) {
                drawPrimitive(PE_FrameMenu, o, p, w);
                return;
            }
            // a plain frame (list, text box, scroll area): a soft thin edge, not Fusion's dark line
            p->save();
            p->setPen(QPen(edge(), 1));
            p->setBrush(Qt::NoBrush);
            p->drawRect(QRectF(o->rect).adjusted(.5, .5, -.5, -.5));
            p->restore();
            return;
        }
        case PE_FrameGroupBox:
        case PE_FrameTabWidget: {
            const QRectF r = QRectF(o->rect).adjusted(.5, .5, -.5, -.5);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(QPen(edge(), 1));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(r, 10, 10);
            p->restore();
            return;
        }
        case PE_FrameTabBarBase: return;
        case PE_FrameLineEdit:
            // an editor inside a combo box is drawn with the box
            return;
        case PE_PanelLineEdit: {
            if (w && qobject_cast<const QWidget *>(w->parentWidget()) && w->parentWidget()->inherits("QComboBox")) return;
            field(p, o, o->rect);
            return;
        }
        case PE_IndicatorCheckBox: {
            const QRectF r = QRectF(o->rect).adjusted(.5, .5, -.5, -.5);
            const bool on = o->state & (State_On | State_NoChange);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            qreal rr = 3 + 3 * look.round;
            if (on) p->setPen(QPen(look.accent, 1)); else p->setPen(Qt::NoPen);
            p->setBrush(on ? look.accent : fieldFill());
            p->drawRoundedRect(r, rr, rr);
            if (on) {
                p->setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                const QRectF c = r.adjusted(.5, .5, -.5, -.5);
                auto pt = [&](qreal x, qreal y) { return QPointF(c.x() + c.width() * x, c.y() + c.height() * y); };
                if (o->state & State_NoChange) p->drawLine(pt(.25, .5), pt(.75, .5));
                else { QPainterPath path; path.moveTo(pt(.24, .52)); path.lineTo(pt(.43, .70)); path.lineTo(pt(.77, .30)); p->drawPath(path); }
            }
            p->restore();
            return;
        }
        case PE_IndicatorRadioButton: {
            const QRectF r = QRectF(o->rect).adjusted(.5, .5, -.5, -.5);
            const bool on = o->state & State_On;
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            if (on) p->setPen(QPen(look.accent, 1)); else p->setPen(Qt::NoPen);
            p->setBrush(on ? look.accent : fieldFill());
            p->drawEllipse(r);
            if (on) { p->setPen(Qt::NoPen); p->setBrush(Qt::white); p->drawEllipse(r.center(), r.width() * .18, r.height() * .18); }
            p->restore();
            (void)en;
            return;
        }
        default: QProxyStyle::drawPrimitive(e, o, p, w);
        }
    }

    void drawControl(ControlElement e, const QStyleOption *o, QPainter *p, const QWidget *w) const override {
        switch (e) {
        case CE_PushButtonLabel: {
            const auto *b = qstyleoption_cast<const QStyleOptionButton *>(o);
            if (b && (b->features & QStyleOptionButton::DefaultButton)) {
                QStyleOptionButton c(*b);
                c.palette.setColor(QPalette::ButtonText, Qt::white);
                QProxyStyle::drawControl(e, &c, p, w);
                return;
            }
            break;
        }
        case CE_MenuItem: {
            const auto *m = qstyleoption_cast<const QStyleOptionMenuItem *>(o);
            if (!m) break;
            using T = QStyleOptionMenuItem;
            if (m->menuItemType == T::Separator) {
                QColor c = pal.color(QPalette::Text);
                c.setAlphaF(0.14);
                p->fillRect(QRectF(o->rect.left() + 12, o->rect.center().y(), o->rect.width() - 24, 1), c);
                return;
            }
            if (m->menuItemType != T::Normal && m->menuItemType != T::DefaultItem && m->menuItemType != T::SubMenu) break;
            const bool en = o->state & State_Enabled;
            const QRect r = o->rect;
            const auto *menu = qobject_cast<const QMenu *>(w);
            // the items of a combo box's popup are drawn with the combo box as the widget
            const auto *combo = qobject_cast<const QComboBox *>(w);
            const auto *comboList = qobject_cast<const QListView *>(w);
            if (!comboList && combo) comboList = qobject_cast<const QListView *>(combo->view());
            const bool isCombo = combo || comboList;
            if (comboList && !underlays.contains(comboList)) comboList = nullptr;
            bool sel = en && (o->state & State_Selected);
            if (comboList) {
                const Underlay *u = underlays.value(comboList);
                const QRectF both = u->cur.intersected(QRectF(r));
                sel = en && u->valid && both.height() > r.height() * 0.5;
            } else if (menu) {
                // the card draws the highlight; this item is "selected" while most of the highlight is over it
                const Slide *sl = slideOf(menu);
                const QRectF both = sl->cur.intersected(QRectF(r));
                sel = en && sl->valid && both.height() > r.height() * 0.5;
            }
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            if (sel && !menu && !comboList) {
                p->setPen(Qt::NoPen);
                p->setBrush(look.accent);
                const qreal rr = 8 + 4 * look.round;
                p->drawRoundedRect(QRectF(r).adjusted(0, 1, 0, -1), rr, rr);
            }
            QColor fg = sel ? QColor(Qt::white) : pal.color(en ? QPalette::Active : QPalette::Disabled, QPalette::Text);
            QColor dim = fg;
            dim.setAlphaF(sel ? 0.80 : 0.55);
            // the places of the shell's menus: icon or tick centred 18 px in, text from 36, shortcut 12 px from the edge
            const bool shellRow = menu && !isCombo;
            int left = shellRow ? r.left() + 36 : r.left() + 14, right = shellRow ? r.right() - 12 : r.right() - 14;
            // the check mark is at the far end, the arrow of a submenu too
            if (m->menuItemType == T::SubMenu) {
                QPainterPath arrow;
                const qreal cx = right - 4, cy = r.center().y();
                arrow.moveTo(cx - 3, cy - 5); arrow.lineTo(cx + 2, cy); arrow.lineTo(cx - 3, cy + 5);
                p->setPen(QPen(dim, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p->setBrush(Qt::NoBrush);
                p->drawPath(arrow);
                right -= 18;
            } else if (m->checkType != T::NotCheckable && m->checked) {
                // an item with an icon is ticked at the far end, one without it where the icon would be
                const bool atEnd = !shellRow || !m->icon.isNull();
                QPainterPath tick;
                const qreal cx = atEnd ? (shellRow ? r.right() - 16 : right - 6) : r.left() + 18, cy = r.center().y();
                tick.moveTo(cx - 5.5, cy); tick.lineTo(cx - 1.8, cy + 4); tick.lineTo(cx + 5.5, cy - 4.5);
                p->setPen(QPen(fg, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p->setBrush(Qt::NoBrush);
                p->drawPath(tick);
                if (atEnd) right -= 24;
            } else if (m->checkType != T::NotCheckable && !isCombo && !shellRow) {
                right -= 24;
            }
            if (!m->icon.isNull()) {
                const int is = shellRow ? 18 : pixelMetric(PM_SmallIconSize, o, w);
                const QIcon::Mode mode = !en ? QIcon::Disabled : sel ? QIcon::Selected : QIcon::Normal;
                const QPixmap pm = m->icon.pixmap(is, is, mode, QIcon::Off);
                p->drawPixmap(QRect(shellRow ? r.left() + 9 : left, r.center().y() - is / 2 + 1, is, is), pm);
                if (!shellRow) left += qMax(is, m->maxIconWidth) + 10;
            } else if (m->maxIconWidth > 0 && !isCombo && !shellRow) {
                left += m->maxIconWidth + 10;
            }
            const QString label = m->text.section('\t', 0, 0), key = m->text.section('\t', 1);
            QFont f = m->font;
            if (m->menuItemType == T::DefaultItem) f.setBold(true);
            p->setFont(f);
            const int hide = Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextHideMnemonic;
            if (!key.isEmpty()) {
                p->setPen(dim);
                p->drawText(QRect(left, r.top(), right - left, r.height()), Qt::AlignRight | hide, key);
            }
            p->setPen(fg);
            p->drawText(QRect(left, r.top(), right - left, r.height()), Qt::AlignLeft | hide, label);
            p->restore();
            return;
        }
        case CE_ItemViewItem: {
            const auto *vo = qstyleoption_cast<const QStyleOptionViewItem *>(o);
            const auto *lv = qobject_cast<const QListView *>(w);
            if (!vo || !lv || !underlays.contains(lv)) break;
            // the highlight is the underlay's: no background here, and light text while it is over this row
            QStyleOptionViewItem c(*vo);
            const Underlay *u = underlays.value(lv);
            c.state &= ~State_Selected;
            if (u->valid && u->cur.intersected(QRectF(vo->rect)).height() > vo->rect.height() * 0.5)
                for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) c.palette.setColor(role, Qt::white);
            QProxyStyle::drawControl(e, &c, p, w);
            return;
        }
        // the part of a menu or a combo box popup no item covers: the card is already there
        case CE_MenuEmptyArea: return;
        case CE_ToolBar: {
            QProxyStyle::drawControl(e, o, p, w);
            const auto *bar = qobject_cast<const QToolBar *>(w);
            if (!bar || !glides.contains(bar)) return;
            const Glide &g = glides[bar];
            if (!g.valid || g.a < 0.01) return;
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            QColor c = mix(pal.color(QPalette::Window), pal.color(QPalette::Text), look.dark ? 0.16 : 0.10);
            c.setAlphaF(g.a);
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            const QRectF r = g.cur.adjusted(0, 0, 0, 0);
            const qreal rr = qMin(r.height() / 2, 8 + 4 * look.round);
            p->drawRoundedRect(r, rr, rr);
            p->restore();
            return;
        }
        case CE_TabBarTabShape: {
            const auto *t = qstyleoption_cast<const QStyleOptionTab *>(o);
            if (!t || (t->shape != QTabBar::RoundedNorth && t->shape != QTabBar::RoundedSouth && t->shape != QTabBar::TriangularNorth && t->shape != QTabBar::TriangularSouth)) break;
            const bool sel = t->state & State_Selected, hot = (t->state & State_MouseOver) && (t->state & State_Enabled);
            if (!sel && !hot) return;
            QRectF r = QRectF(t->rect).adjusted(2, 3, -2, -3);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            QColor c = sel ? mix(pal.color(QPalette::Window), pal.color(QPalette::Text), look.dark ? 0.12 : 0.08) : fieldFill();
            p->setBrush(c);
            const qreal rr = qMin(r.height() / 2, 8 + 4 * look.round);
            p->drawRoundedRect(r, rr, rr);
            if (sel) {
                // a short accent bar under the chosen tab
                p->setBrush(look.accent);
                const QRectF bar(r.center().x() - r.width() * 0.2, t->shape == QTabBar::RoundedSouth || t->shape == QTabBar::TriangularSouth ? r.top() + 1 : r.bottom() - 3, r.width() * 0.4, 2.5);
                p->drawRoundedRect(bar, 1.25, 1.25);
            }
            p->restore();
            return;
        }
        case CE_ProgressBarGroove: {
            const auto *pb = qstyleoption_cast<const QStyleOptionProgressBar *>(o);
            const QRectF r = grooveRect(o->rect, pb && pb->textVisible);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            p->setBrush(mix(pal.color(QPalette::Window), pal.color(QPalette::Text), 0.12));
            p->drawRoundedRect(r, r.height() / 2, r.height() / 2);
            p->restore();
            return;
        }
        case CE_ProgressBarContents: {
            const auto *b = qstyleoption_cast<const QStyleOptionProgressBar *>(o);
            if (!b || b->maximum == b->minimum) break;  // the busy bar is Fusion's
            const QRectF g = grooveRect(o->rect, b->textVisible);
            const qreal f = qreal(b->progress - b->minimum) / (b->maximum - b->minimum);
            const bool h = o->state & State_Horizontal;
            QRectF r = g;
            if (h) r.setWidth(qMax(g.height(), g.width() * f)); else { r.setTop(g.bottom() - qMax(g.width(), g.height() * f)); }
            if (f <= 0) return;
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            p->setBrush(look.accent);
            const qreal rr = qMin(r.width(), r.height()) / 2;
            p->drawRoundedRect(r, rr, rr);
            p->restore();
            return;
        }
        case CE_ProgressBarLabel: {
            const auto *b = qstyleoption_cast<const QStyleOptionProgressBar *>(o);
            if (!b || !b->textVisible || !(o->state & State_Horizontal) || b->maximum == b->minimum) break;
            // the text sits in the bar: light where the colour is under it, ink on the groove
            const QRectF g = grooveRect(o->rect, true);
            const qreal f = qreal(b->progress - b->minimum) / (b->maximum - b->minimum);
            const QRectF done(g.x(), g.y(), qMax(g.height(), g.width() * f), g.height());
            p->save();
            QFont fn = p->font();
            fn.setPixelSize(11);
            p->setFont(fn);
            p->setClipRect(done);
            p->setPen(Qt::white);
            p->drawText(g, Qt::AlignCenter, b->text);
            p->setClipRect(QRectF(done.right(), g.y(), g.right() - done.right(), g.height()));
            p->setPen(pal.color(QPalette::Text));
            p->drawText(g, Qt::AlignCenter, b->text);
            p->restore();
            return;
        }
        // the header of a table or tree: flat, a hairline below, a faint divider between sections
        case CE_HeaderSection: {
            p->save();
            p->fillRect(o->rect, pal.color(QPalette::Base));
            const bool hot = (o->state & State_MouseOver) || (o->state & State_Sunken);
            if (hot) p->fillRect(o->rect, mix(pal.color(QPalette::Base), pal.color(QPalette::Text), 0.06));
            p->setPen(QPen(edge(), 1));
            p->drawLine(o->rect.bottomLeft(), o->rect.bottomRight());
            QColor div = edge();
            div.setAlphaF(div.alphaF() * 0.6);
            p->setPen(div);
            const auto *h = qstyleoption_cast<const QStyleOptionHeader *>(o);
            if (!h || h->position != QStyleOptionHeader::End)
                p->drawLine(o->rect.right(), o->rect.top() + 6, o->rect.right(), o->rect.bottom() - 6);
            p->restore();
            return;
        }
        case CE_HeaderEmptyArea:
            p->fillRect(o->rect, pal.color(QPalette::Base));
            p->setPen(QPen(edge(), 1));
            p->drawLine(o->rect.bottomLeft(), o->rect.bottomRight());
            return;
        // a name in the menu bar: a soft pill while the pointer is on it or its menu is open
        case CE_MenuBarItem: {
            const auto *m = qstyleoption_cast<const QStyleOptionMenuItem *>(o);
            if (!m) break;
            const bool on = (o->state & State_Selected) || (o->state & State_Sunken);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            if (on && (o->state & State_Enabled)) {
                p->setPen(Qt::NoPen);
                p->setBrush(mix(pal.color(QPalette::Window), pal.color(QPalette::Text), look.dark ? 0.16 : 0.10));
                p->drawRoundedRect(QRectF(o->rect).adjusted(1, 3, -1, -3), 8, 8);
            }
            p->setPen(pal.color((o->state & State_Enabled) ? QPalette::Active : QPalette::Disabled, QPalette::Text));
            p->setFont(m->font);
            p->drawText(o->rect, Qt::AlignCenter | Qt::TextHideMnemonic | Qt::TextSingleLine, m->text);
            p->restore();
            return;
        }
        default: break;
        }
        QProxyStyle::drawControl(e, o, p, w);
    }

    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *o, QPainter *p, const QWidget *w) const override {
        if (cc == CC_ScrollBar) {
            const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(o);
            if (!sb) return;
            const bool h = sb->orientation == Qt::Horizontal;
            const bool hot = (o->state & State_MouseOver) || (sb->activeSubControls & SC_ScrollBarSlider);
            const bool inView = w && qobject_cast<const QAbstractScrollArea *>(w->parentWidget());
            p->fillRect(o->rect, pal.color(inView ? QPalette::Base : QPalette::Window));
            if (sb->maximum <= sb->minimum) return;
            QRectF s = subControlRect(CC_ScrollBar, o, SC_ScrollBarSlider, w);
            const qreal thick = hot ? 8 : 5;
            if (h) s = QRectF(s.x() + 2, s.center().y() - thick / 2, s.width() - 4, thick);
            else s = QRectF(s.center().x() - thick / 2, s.y() + 2, thick, s.height() - 4);
            QColor c = pal.color(QPalette::Text);
            c.setAlphaF((o->state & State_Sunken) || (sb->activeSubControls & SC_ScrollBarSlider) ? 0.55 : hot ? 0.42 : 0.28);
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            p->setBrush(c);
            p->drawRoundedRect(s, thick / 2, thick / 2);
            p->restore();
            return;
        }
        if (cc == CC_Slider) {
            const auto *sl = qstyleoption_cast<const QStyleOptionSlider *>(o);
            if (!sl) return;
            const bool h = sl->orientation == Qt::Horizontal;
            const QRect gr = subControlRect(cc, o, SC_SliderGroove, w), hr = subControlRect(cc, o, SC_SliderHandle, w);
            const qreal t = 6;
            QRectF groove = h ? QRectF(gr.x(), gr.center().y() - t / 2 + .5, gr.width(), t) : QRectF(gr.center().x() - t / 2 + .5, gr.y(), t, gr.height());
            QRectF done = groove;
            if (h) done.setRight(hr.center().x()); else done.setTop(hr.center().y());
            if (!h) done = QRectF(groove.x(), hr.center().y(), t, groove.bottom() - hr.center().y());
            const bool en = o->state & State_Enabled;
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            p->setPen(Qt::NoPen);
            p->setBrush(mix(pal.color(QPalette::Window), pal.color(QPalette::Text), 0.14));
            p->drawRoundedRect(groove, t / 2, t / 2);
            if (en) { p->setBrush(look.accent); p->drawRoundedRect(done, t / 2, t / 2); }
            const QRectF knob = QRectF(hr).adjusted(0, 0, -1, -1);
            if (look.depth > 0) { p->setBrush(QColor(0, 0, 0, look.dark ? 90 : 40)); p->drawEllipse(knob.translated(0, 1)); }
            p->setPen(QPen(edge(), 1));
            p->setBrush(look.dark ? mix(pal.color(QPalette::Button), Qt::white, 0.55) : QColor(Qt::white));
            p->drawEllipse(knob);
            p->restore();
            return;
        }
        if (cc == CC_SpinBox) {
            const auto *sb = qstyleoption_cast<const QStyleOptionSpinBox *>(o);
            if (!sb) return;
            field(p, o, o->rect);
            const bool en = o->state & State_Enabled;
            for (auto part : {SC_SpinBoxUp, SC_SpinBoxDown}) {
                if (!(sb->subControls & part) || sb->buttonSymbols == QAbstractSpinBox::NoButtons) continue;
                const bool up = part == SC_SpinBoxUp;
                const bool stepOn = up ? (sb->stepEnabled & QAbstractSpinBox::StepUpEnabled) : (sb->stepEnabled & QAbstractSpinBox::StepDownEnabled);
                const QRect r = subControlRect(cc, o, part, w);
                const bool hot = en && (sb->activeSubControls & part);
                const bool down = hot && (o->state & State_Sunken);
                if (hot) {
                    p->save();
                    p->setRenderHint(QPainter::Antialiasing);
                    p->setPen(Qt::NoPen);
                    QColor hc = pal.color(QPalette::Text);
                    hc.setAlphaF(down ? 0.18 : 0.10);
                    p->setBrush(hc);
                    p->drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 6, 6);
                    p->restore();
                }
                QColor fg = pal.color(QPalette::Text);
                fg.setAlphaF(!en || !stepOn ? 0.25 : hot ? 0.9 : 0.6);
                QPainterPath chev;
                const QPointF c = QRectF(r).center();
                const qreal d = up ? 1.5 : -1.5;
                chev.moveTo(c.x() - 3.5, c.y() + d); chev.lineTo(c.x(), c.y() - d); chev.lineTo(c.x() + 3.5, c.y() + d);
                p->save();
                p->setRenderHint(QPainter::Antialiasing);
                p->setPen(QPen(fg, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p->setBrush(Qt::NoBrush);
                p->drawPath(chev);
                p->restore();
            }
            return;
        }
        if (cc == CC_ComboBox) {
            const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(o);
            if (!cb) return;
            if (cb->editable) field(p, o, o->rect); else key(p, o, o->rect, QColor());
            QStyleOption a;
            a.QStyleOption::operator=(*o);
            a.rect = subControlRect(cc, o, SC_ComboBoxArrow, w).adjusted(0, 0, -4, 0);
            QProxyStyle::drawPrimitive(PE_IndicatorArrowDown, &a, p, w);
            return;
        }
        QProxyStyle::drawComplexControl(cc, o, p, w);
    }

private:
    Look look;
    QPalette pal;

    // A list with one chosen row (the pages of a settings dialog, a list of files): the chosen row's highlight slides to
    // the next one. It is painted by a widget under the list's viewport, which is made see-through for that.
    struct Underlay : QWidget {
        const ChebriumStyle *st;
        QListView *view;
        QRectF cur, last;
        bool valid = false;
        QVariantAnimation *anim;

        Underlay(const ChebriumStyle *s, QListView *v) : QWidget(v), st(s), view(v) {
            setAttribute(Qt::WA_TransparentForMouseEvents);
            anim = new QVariantAnimation(this);
            anim->setDuration(160);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &x) { cur = x.toRectF(); update(); view->viewport()->update(); });
        }

        QRectF target() const {
            const QModelIndex i = view->currentIndex();
            if (!i.isValid() || !view->selectionModel() || !view->selectionModel()->isSelected(i)) return QRectF();
            return QRectF(view->visualRect(i));
        }

        // `slide`: glide there; else (a scroll, a resize) jump
        void follow(bool slide) {
            const QRectF t = target();
            if (t.isNull()) { valid = false; anim->stop(); update(); return; }
            if (t == last && anim->state() == QAbstractAnimation::Running) return;
            const bool moved = t != last;
            last = t;
            if (!valid || !slide || !st->look.slide) { anim->stop(); cur = t; valid = true; update(); view->viewport()->update(); return; }
            if (!moved) return;
            anim->stop();
            anim->setStartValue(cur);
            anim->setEndValue(t);
            anim->start();
        }

        void paintEvent(QPaintEvent *) override {
            QPainter p(this);
            if (!view->inherits("QComboBoxListView")) p.fillRect(rect(), st->pal.color(QPalette::Base));
            if (!valid) return;
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(st->look.accent);
            const qreal rr = 8 + 4 * st->look.round;
            p.drawRoundedRect(cur.adjusted(3, 1, -3, -1), qMin(rr, cur.height() / 2 - 1), qMin(rr, cur.height() / 2 - 1));
        }
    };
    QHash<const QWidget *, Underlay *> underlays;
    bool restoring = false;

    static bool slidable(const QListView *v) {
        return v->selectionMode() == QAbstractItemView::SingleSelection && v->viewMode() == QListView::ListMode && !v->isWrapping() && !v->inherits("QFileDialog");
    }

    void attachSlide(QListView *lv) {
        if (underlays.contains(lv)) return;
        auto *u = new Underlay(this, lv);
        underlays.insert(lv, u);
        lv->viewport()->setAutoFillBackground(false);
        if (lv->inherits("QComboBoxListView")) lv->setFrameShape(QFrame::NoFrame);
        lv->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
        u->setGeometry(lv->viewport()->geometry());
        u->stackUnder(lv->viewport());
        u->show();
        lv->installEventFilter(this);
        lv->viewport()->installEventFilter(this);
        connect(lv->verticalScrollBar(), &QScrollBar::valueChanged, u, [u] { u->follow(false); });
        connect(lv, &QObject::destroyed, this, [this, lv] { underlays.remove(lv); });
        u->follow(false);
    }

    void detachSlide(QListView *lv) {
        if (Underlay *u = underlays.take(lv)) {
            lv->viewport()->setAutoFillBackground(true);
            lv->removeEventFilter(this);
            lv->viewport()->removeEventFilter(this);
            delete u;
        }
    }

    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() == QEvent::ApplicationPaletteChange && o == QCoreApplication::instance() && !restoring) {
            const QPalette now = QGuiApplication::palette();
            for (auto role : {QPalette::Window, QPalette::Base, QPalette::Text, QPalette::Button, QPalette::Highlight}) {
                if (now.color(role) != pal.color(role)) {
                    restoring = true;
                    QGuiApplication::setPalette(pal);
                    restoring = false;
                    break;
                }
            }
            return QProxyStyle::eventFilter(o, e);
        }
        if (e->type() == QEvent::Enter || e->type() == QEvent::Leave) {
            if (auto *tb = qobject_cast<QToolButton *>(o); tb && qobject_cast<QToolBar *>(tb->parentWidget())) {
                if (e->type() == QEvent::Enter && tb->isEnabled()) glideTo(tb); else glideAway(tb);
            }
        }
        QListView *lv = qobject_cast<QListView *>(o);
        if (!lv) {
            if (auto *vp = qobject_cast<QWidget *>(o); vp && vp->parentWidget()) lv = qobject_cast<QListView *>(vp->parentWidget());
        }
        if (lv && underlays.contains(lv)) {
            Underlay *u = underlays.value(lv);
            switch (e->type()) {
            case QEvent::Resize:
            case QEvent::Move:
            case QEvent::Show:
            case QEvent::LayoutRequest:
                u->setGeometry(lv->viewport()->geometry());
                u->follow(false);
                break;
            case QEvent::Paint:
                if (o == lv->viewport()) u->follow(true);
                break;
            default: break;
            }
        }
        return QProxyStyle::eventFilter(o, e);
    }

    // The highlight of a tool bar: one soft pill the toolbar draws behind its buttons, gliding to the button under the pointer
    // and fading out when the pointer leaves them all.
    struct Glide {
        QVariantAnimation *move = nullptr, *fade = nullptr;
        QRectF cur, to;
        qreal a = 0;
        bool valid = false;
    };
    mutable QHash<const QWidget *, Glide> glides;

    Glide &glideOf(QToolBar *bar) {
        Glide &g = glides[bar];
        if (!g.move) {
            g.move = new QVariantAnimation(bar);
            g.move->setDuration(150);
            g.move->setEasingCurve(QEasingCurve::OutCubic);
            connect(g.move, &QVariantAnimation::valueChanged, bar, [this, bar](const QVariant &v) { glides[bar].cur = v.toRectF(); bar->update(); });
            g.fade = new QVariantAnimation(bar);
            g.fade->setDuration(130);
            connect(g.fade, &QVariantAnimation::valueChanged, bar, [this, bar](const QVariant &v) { glides[bar].a = v.toDouble(); bar->update(); });
            connect(g.fade, &QVariantAnimation::finished, bar, [this, bar] { if (glides[bar].a < 0.01) glides[bar].valid = false; bar->update(); });
            connect(bar, &QObject::destroyed, this, [this, bar] { glides.remove(bar); });
        }
        return g;
    }
    void glideTo(QToolButton *b) {
        auto *bar = qobject_cast<QToolBar *>(b->parentWidget());
        Glide &g = glideOf(bar);
        const QRectF target = b->geometry();
        if (!g.valid || g.a < 0.05) {
            g.move->stop();
            g.cur = g.to = target;
            g.valid = true;
        } else if (target != g.to) {
            g.move->stop();
            g.move->setStartValue(g.cur);
            g.move->setEndValue(target);
            g.to = target;
            if (look.slide) g.move->start(); else g.cur = target;
        }
        g.fade->stop();
        g.fade->setStartValue(g.a);
        g.fade->setEndValue(1.0);
        g.fade->start();
        bar->update();
    }
    void glideAway(QToolButton *b) {
        auto *bar = qobject_cast<QToolBar *>(b->parentWidget());
        Glide &g = glideOf(bar);
        if (!g.valid) return;
        // the pointer may be going straight to the next button: it enters it a moment later, which takes the fade back
        g.fade->stop();
        g.fade->setStartValue(g.a);
        g.fade->setEndValue(0.0);
        g.fade->start();
    }

    // The highlight of a menu slides from item to item (the shell's "Sliding highlight"): the card draws it at an animated
    // place, the items only read how much of themselves it covers. A menu that has just opened, or has no active item,
    // shows the highlight at once.
    struct Slide {
        QVariantAnimation *anim = nullptr;
        QRectF cur, to;
        bool valid = false;
    };
    mutable QHash<const QWidget *, Slide> slides;

    const Slide *slideOf(const QMenu *m) const {
        Slide &s = slides[m];
        QAction *act = m->activeAction();
        const bool on = act && act->isEnabled() && !act->isSeparator();
        if (!on) { s.valid = false; if (s.anim) s.anim->stop(); return &s; }
        const QRectF target = m->actionGeometry(act);
        if (!s.anim) {
            QMenu *menu = const_cast<QMenu *>(m);
            s.anim = new QVariantAnimation(menu);
            s.anim->setDuration(150);
            s.anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(s.anim, &QVariantAnimation::valueChanged, menu, [this, menu](const QVariant &v) { slides[menu].cur = v.toRectF(); menu->update(); });
            // Qt itself only repaints the rows the active item left and entered: the card moves the highlight over all the rows,
            // so the whole menu is painted again when the slide ends, or a stripe of an earlier frame could stay behind
            connect(s.anim, &QVariantAnimation::finished, menu, [this, menu] { slides[menu].cur = slides[menu].to; menu->update(); });
            connect(menu, &QObject::destroyed, this, [this, menu] { slides.remove(menu); });
        }
        if (!s.valid) {
            s.cur = s.to = target;
            s.valid = true;
        } else if (target != s.to && !look.slide) {
            s.cur = s.to = target;
        } else if (target != s.to) {
            s.anim->stop();
            s.anim->setStartValue(s.cur);
            s.anim->setEndValue(target);
            s.to = target;
            s.anim->start();
            const_cast<QMenu *>(m)->update();
        }
        return &s;
    }

    // The look of liquid glass without seeing through it: a body with a soft vertical tone, a darker rim just inside the edge that
    // reads as the thickness of the pane, a bright bevel line inside it, and light gathering in the lower corners
    void glassCard(QPainter *p, const QRectF &r, qreal rr, const QColor &fill) const {
        const bool dk = look.dark;
        const Tune &t = tune();
        auto alpha = [](int a, qreal k) { return qBound(0, int(a * k + 0.5), 255); };
        // lightness of a colour moved by `pc` percent points, and the accent let into it
        auto shade = [&](QColor c, qreal pc) {
            float h, sa, l, al;
            c.getHslF(&h, &sa, &l, &al);
            c.setHslF(h < 0 ? 0 : h, sa, qBound(0.0, l + pc / 100.0, 1.0), al);
            return c;
        };
        auto tinted = [&](const QColor &c) { return t.tint > 0 ? mix(c, look.accent, t.tint * 0.35) : c; };
        const QColor base = tinted(shade(fill, t.fill));
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QLinearGradient body(r.topLeft(), r.bottomLeft());
        body.setColorAt(0, shade(dk ? base.lighter(108) : base, t.top));
        body.setColorAt(1, shade(dk ? base.darker(112) : mix(base, QColor(226, 229, 245), 0.55), t.bottom));
        p->setPen(Qt::NoPen);
        p->setBrush(body);
        p->drawRoundedRect(r, rr, rr);
        p->setClipPath([&] { QPainterPath c; c.addRoundedRect(r, rr, rr); return c; }());
        // the thickness of the glass: a narrow tinted band by the edge, only a hint
        for (int i = 0; i < 3; i++) {
            QColor c = dk ? QColor(12, 12, 12) : look.accent.lighter(135);
            c.setAlphaF(qBound(0.0, (dk ? 0.16 : 0.06) * (1.0 - i / 3.0) * t.band, 1.0));
            p->setPen(QPen(c, 1));
            p->setBrush(Qt::NoBrush);
            const QRectF q = r.adjusted(i + .5, i + .5, -i - .5, -i - .5);
            p->drawRoundedRect(q, qMax(1.0, rr - i), qMax(1.0, rr - i));
        }
        // light caught by the lower corners and the bottom edge
        for (const QPointF c : {r.bottomLeft(), r.bottomRight()}) {
            QRadialGradient g(c, rr * 3.2);
            g.setColorAt(0, QColor(255, 255, 255, alpha(dk ? 40 : 190, t.glow)));
            g.setColorAt(1, QColor(255, 255, 255, 0));
            p->setPen(Qt::NoPen);
            p->setBrush(g);
            p->drawRect(r);
        }
        // a sheen across the top
        QLinearGradient sheen(r.topLeft(), r.topLeft() + QPointF(0, qMin(r.height() * 0.45, 46.0)));
        sheen.setColorAt(0, QColor(255, 255, 255, alpha(dk ? 14 : 150, t.sheen)));
        sheen.setColorAt(1, QColor(255, 255, 255, 0));
        p->setBrush(sheen);
        p->drawRect(r);
        p->setClipping(false);
        // the outline: a crisp line on the very edge, light where the light hits the glass (top left, bottom right) and
        // faint between; a second line a pixel inside makes the pane look thick
        QLinearGradient out(r.topLeft(), r.bottomRight());
        out.setColorAt(0, QColor(255, 255, 255, alpha(dk ? 190 : 255, t.outline)));
        out.setColorAt(0.5, QColor(255, 255, 255, alpha(dk ? 70 : 150, t.outline)));
        out.setColorAt(1, QColor(255, 255, 255, alpha(dk ? 160 : 235, t.outline)));
        p->setBrush(Qt::NoBrush);
        // the dark hairline outside it keeps the edge readable on a light backdrop
        p->setPen(QPen(dk ? QColor(0, 0, 0, alpha(120, t.hairline)) : QColor(40, 48, 80, alpha(105, t.hairline)), 1));
        p->drawRoundedRect(r, rr, rr);
        const qreal w = qMax(0.5, t.width), o = w / 2 + 0.5;
        p->setPen(QPen(QBrush(out), w));
        p->drawRoundedRect(r.adjusted(o, o, -o, -o), qMax(1.0, rr - o), qMax(1.0, rr - o));
        p->setPen(QPen(dk ? QColor(255, 255, 255, alpha(34, t.inner)) : QColor(255, 255, 255, alpha(140, t.inner)), 1));
        const qreal q = w + 1.5;
        p->drawRoundedRect(r.adjusted(q, q, -q, -q), qMax(1.0, rr - q), qMax(1.0, rr - q));
        p->restore();
    }

    // popups (menus, tool tips) get round corners, so the window behind them is see-through
    static bool roundPopup(const QWidget *w) { return w->inherits("QTipLabel") || w->inherits("QComboBoxPrivateContainer") || qobject_cast<const QMenu *>(w); }

    // popups are solid: the glass look is painted (glassCard), nothing behind them shows through,
    // unless the theme asks for a see-through popup with popup_opacity
    qreal popupAlpha() const { return look.popupOpacity > 0.0 ? look.popupOpacity : 1.0; }

    QColor popupFill() const { QColor c = look.dark ? QColor(33, 33, 33) : QColor(255, 255, 255); c.setAlphaF(popupAlpha()); return c; }

    void load() { look = readLook(); pal = makePalette(look); darkTune = darkDefault(); lightTune = Tune(); readGlass(darkTune, lightTune); }
    Tune darkTune, lightTune;
    const Tune &tune() const { return look.dark ? darkTune : lightTune; }

    // Tell Qt whether the desktop is light or dark: programs that follow it (Kate's automatic editor theme, KDE's scheme manager,
    // web views) would otherwise ask the desktop portal, which knows nothing of this shell's look
    void applyScheme() const {
        if (auto *hints = QGuiApplication::styleHints()) hints->setColorScheme(look.dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
    }

    // the fill of fields, boxes and the unchecked indicators: a little off the window towards the ink
    QColor fieldFill() const { return mix(pal.color(QPalette::Window), pal.color(QPalette::Text), look.dark ? 0.08 : 0.06); }

    QColor edge() const { return look.dark ? QColor(255, 255, 255, 38) : QColor(0, 0, 0, 56); }

    qreal corner(const QRectF &r, qreal cap) const { return qMin(qMin(r.height() / 2, cap), 4 + (r.height() / 2 - 4) * look.round); }

    QRectF grooveRect(const QRect &r, bool withText = false) const {
        const bool h = r.width() >= r.height();
        const qreal t = withText && h ? 16 : 6;
        return h ? QRectF(r.x(), r.center().y() - t / 2 + .5, r.width(), t) : QRectF(r.center().x() - t / 2 + .5, r.y(), t, r.height());
    }

    // a raised key: a soft shadow, a face a little lighter at the top, a thin edge
    void key(QPainter *p, const QStyleOption *o, const QRect &rect, QColor fill) const {
        const bool en = o->state & State_Enabled, down = o->state & State_Sunken, hot = o->state & State_MouseOver;
        const bool lifted = look.depth > 0 && !look.flat && en;
        QColor base = fill.isValid() ? fill : pal.color(QPalette::Button);
        if (!en) base = mix(pal.color(QPalette::Window), base, 0.6);
        else if (down) base = look.dark ? base.lighter(112) : base.darker(108);
        else if (hot) base = look.dark ? base.lighter(110) : mix(base, look.accent, fill.isValid() ? 0.0 : 0.06).lighter(fill.isValid() ? 108 : 100);
        QRectF f = QRectF(rect).adjusted(.5, .5, -.5, lifted ? -1.5 : -.5);
        const qreal rr = corner(f, 1e9);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen);
        if (lifted && !down) {
            p->setBrush(QColor(0, 0, 0, look.dark ? int(80 * qMin(look.depth, 1.0)) : int(36 * look.depth)));
            p->drawRoundedRect(f.translated(0, 1), rr, rr);
        }
        if (lifted) {
            QLinearGradient g(f.topLeft(), f.bottomLeft());
            g.setColorAt(0, base.lighter(100 + int(7 * look.depth)));
            g.setColorAt(1, base.darker(100 + int(3 * look.depth)));
            p->setBrush(g);
        } else p->setBrush(base);
        if (fill.isValid() && fill == look.accent) p->setPen(QPen(fill.darker(112), 1)); else p->setPen(Qt::NoPen);
        p->drawRoundedRect(f, rr, rr);
        if ((o->state & State_HasFocus) && (o->state & State_KeyboardFocusChange)) {
            QColor c = look.accent; c.setAlpha(150);
            p->setPen(QPen(c, 2)); p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(f.adjusted(-1, -1, 1, 1), rr + 1, rr + 1);
        }
        p->restore();
    }

    // a field to type in: the base colour, an edge that turns to the accent while it has the focus
    void field(QPainter *p, const QStyleOption *o, const QRect &rect) const {
        QRectF f = QRectF(rect).adjusted(.5, .5, -.5, -.5);
        const qreal rr = corner(f, 10);
        const bool focus = o->state & State_HasFocus;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(fieldFill());
        if (focus) p->setPen(QPen(look.accent, 1.6)); else p->setPen(Qt::NoPen);
        p->drawRoundedRect(f, rr, rr);
        p->restore();
    }
};

class ChebriumPlugin : public QStylePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QStyleFactoryInterface" FILE "chebrium.json")
public:
    QStyle *create(const QString &key) override { return key.compare("chebrium", Qt::CaseInsensitive) == 0 ? new ChebriumStyle : nullptr; }
};

#include "chebrium-style.moc"
