// chebrium-tweak: sliders for the glass of popups, live. Every change is written to ~/.config/memrium/glass.toml at once;
// the style in every running Qt program watches that file and repaints, so a menu open in Prism changes as a slider moves.
// The window on the right is a menu of this program, painted by the same style, to look at while turning.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

struct Knob { const char *key, *title, *hint; double lo, hi, def; };

static const Knob knobs[] = {
    {"outline", "Outline", "the crisp light line on the edge", 0, 2, 1},
    {"width", "Outline width", "px", 0.5, 3, 1},
    {"hairline", "Dark hairline", "the thin line outside the outline", 0, 2.5, 1},
    {"inner", "Inner line", "a second line a little inside", 0, 3, 1},
    {"band", "Edge band", "the soft shade along the edge", 0, 4, 1},
    {"glow", "Corner glow", "light in the lower corners", 0, 3, 1},
    {"sheen", "Top sheen", "light across the top", 0, 3, 1},
    {"fill", "Brightness", "lightness of the whole pane, %", -30, 30, 0},
    {"top", "Top tone", "lighter or darker at the top, %", -30, 30, 0},
    {"bottom", "Bottom tone", "lighter or darker at the bottom, %", -30, 30, 0},
    {"tint", "Accent tint", "how much of the accent colours the glass", 0, 1, 0},
    {"radius", "Corner radius", "added to the corners, px", -8, 14, 0},
};
static const int N = sizeof(knobs) / sizeof(knobs[0]);
// the dark theme ships with these (the same numbers as darkDefault() in the style), in the order of `knobs`
static const double darkDef[] = {0.13, 3, 0.29, 0.24, 1.53, 1.36, 2.0, -8, 7, -0.4, 0, -3.75};
static double defOf(int mode, int i) { return mode == 0 ? darkDef[i] : knobs[i].def; }

static QString glassPath() {
    QByteArray env = qgetenv("CHEBRIUM_APPEARANCE");
    QString dir = !env.isEmpty() ? QFileInfo(QString::fromLocal8Bit(env)).absolutePath() : [] {
        QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
        if (base.isEmpty()) base = QDir::homePath() + "/.config";
        return base + "/memrium-de";
    }();
    QDir().mkpath(dir);
    return dir + "/glass.toml";
}

class Tweak : public QWidget {
public:
    Tweak() {
        setWindowTitle("Chebrium glass");
        for (int m = 0; m < 2; m++) for (int i = 0; i < N; i++) val[m][i] = defOf(m, i);
        read();
        auto *root = new QHBoxLayout(this);
        auto *left = new QVBoxLayout;
        root->addLayout(left, 1);

        auto *top = new QHBoxLayout;
        top->addWidget(new QLabel("Editing"));
        mode = new QComboBox;
        mode->addItems({"Dark theme", "Light theme"});
        top->addWidget(mode, 1);
        left->addLayout(top);

        auto *area = new QScrollArea;
        area->setWidgetResizable(true);
        area->setFrameShape(QFrame::NoFrame);
        auto *inner = new QWidget;
        auto *col = new QVBoxLayout(inner);
        for (int i = 0; i < N; i++) {
            auto *title = new QLabel(QString("%1  <small style='color:gray'>%2</small>").arg(knobs[i].title, knobs[i].hint));
            num[i] = new QLabel;
            num[i]->setMinimumWidth(56);
            num[i]->setAlignment(Qt::AlignRight);
            auto *row = new QHBoxLayout;
            row->addWidget(title, 1);
            row->addWidget(num[i]);
            sl[i] = new QSlider(Qt::Horizontal);
            sl[i]->setRange(0, 1000);
            col->addLayout(row);
            col->addWidget(sl[i]);
            connect(sl[i], &QSlider::valueChanged, this, [this, i](int v) {
                val[cur()][i] = knobs[i].lo + (knobs[i].hi - knobs[i].lo) * v / 1000.0;
                showNum(i);
                queueSave();
            });
        }
        col->addStretch(1);
        area->setWidget(inner);
        left->addWidget(area, 1);

        auto *buttons = new QHBoxLayout;
        auto *reset = new QPushButton("Reset this theme");
        auto *copy = new QPushButton("Copy the other theme");
        buttons->addWidget(reset);
        buttons->addWidget(copy);
        left->addLayout(buttons);
        connect(reset, &QPushButton::clicked, this, [this] { for (int i = 0; i < N; i++) val[cur()][i] = defOf(cur(), i); sync(); queueSave(); });
        connect(copy, &QPushButton::clicked, this, [this] { for (int i = 0; i < N; i++) val[cur()][i] = val[1 - cur()][i]; sync(); queueSave(); });
        connect(mode, &QComboBox::currentIndexChanged, this, [this] { sync(); });

        // a menu to look at: a window of its own, so it stays while the sliders are used
        preview = new QMenu(this);
        preview->setWindowFlags(Qt::Widget);
        // a menu closes itself on many things (the style being reloaded, an action, losing focus): this one stays
        preview->installEventFilter(this);
        preview->addAction(QIcon::fromTheme("document-open"), "Open")->setShortcut(QKeySequence("Ctrl+O"));
        auto *c = preview->addAction(QIcon::fromTheme("document-save"), "Save");
        c->setShortcut(QKeySequence("Ctrl+S"));
        auto *ck = preview->addAction("A ticked item");
        ck->setCheckable(true);
        ck->setChecked(true);
        preview->addSeparator();
        preview->addAction("Disabled")->setEnabled(false);
        preview->addMenu("Submenu")->addAction("One");
        preview->addAction(QIcon::fromTheme("application-exit"), "Quit")->setShortcut(QKeySequence("Ctrl+Q"));
        preview->setActiveAction(c);
        auto *right = new QVBoxLayout;
        right->addWidget(new QLabel("Preview"));
        right->addWidget(preview, 0, Qt::AlignTop);
        right->addStretch(1);
        root->addLayout(right);

        save = new QTimer(this);
        save->setSingleShot(true);
        save->setInterval(40);
        connect(save, &QTimer::timeout, this, [this] { write(); });
        sync();
        resize(640, 640);
    }
    void showPreview() {
        if (!preview->isVisible()) preview->show();
        if (!preview->activeAction()) preview->setActiveAction(preview->actions().at(1));
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        if (o == preview && (e->type() == QEvent::Hide || e->type() == QEvent::Close) && isVisible())
            QTimer::singleShot(0, this, [this] { showPreview(); });
        return QWidget::eventFilter(o, e);
    }

private:
    int cur() const { return mode->currentIndex(); }
    void showNum(int i) {
        const double v = val[cur()][i];
        num[i]->setText(QString::number(v, 'f', knobs[i].hi - knobs[i].lo > 20 ? 0 : 2));
    }
    void sync() {
        for (int i = 0; i < N; i++) {
            QSignalBlocker b(sl[i]);
            sl[i]->setValue(int(std::lround((val[cur()][i] - knobs[i].lo) / (knobs[i].hi - knobs[i].lo) * 1000)));
            showNum(i);
        }
    }
    void queueSave() { save->start(); }
    void read() {
        QFile f(glassPath());
        if (!f.open(QIODevice::ReadOnly)) return;
        while (!f.atEnd()) {
            QString line = QString::fromUtf8(f.readLine()).trimmed();
            int eq = line.indexOf('=');
            if (line.startsWith('#') || eq < 0) continue;
            const QString k = line.left(eq).trimmed();
            const int m = k.startsWith("dark_") ? 0 : k.startsWith("light_") ? 1 : -1;
            if (m < 0) continue;
            for (int i = 0; i < N; i++) if (k.mid(k.indexOf('_') + 1) == knobs[i].key) val[m][i] = line.mid(eq + 1).trimmed().toDouble();
        }
    }
    void write() {
        QSaveFile f(glassPath());
        if (!f.open(QIODevice::WriteOnly)) return;
        f.write("# written by chebrium-tweak; 1 (0 for the offsets) is the look the style ships with\n");
        for (int m = 0; m < 2; m++)
            for (int i = 0; i < N; i++) f.write(QString("%1_%2 = %3\n").arg(m ? "light" : "dark", knobs[i].key).arg(val[m][i], 0, 'f', 3).toUtf8());
        f.commit();
    }

    QComboBox *mode;
    QSlider *sl[N];
    QLabel *num[N];
    double val[2][N];
    QMenu *preview;
    QTimer *save;
};

int main(int argc, char **argv) {
    QApplication a(argc, argv);
    Tweak w;
    w.show();
    QTimer::singleShot(150, [&w] { w.showPreview(); });
    return a.exec();
}
