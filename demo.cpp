// A window of the common widgets; with a file name argument it saves a picture of itself and quits.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QToolTip>
#include <QToolButton>

int main(int argc, char **argv) {
    QApplication a(argc, argv);
    QWidget w;
    w.setWindowTitle("chebrium style");
    auto *g = new QGridLayout(&w);
    auto *ok = new QPushButton("Run"); ok->setDefault(true);
    auto *off = new QPushButton("Disabled"); off->setEnabled(false);
    auto *tog = new QPushButton("Toggled"); tog->setCheckable(true); tog->setChecked(true);
    g->addWidget(ok, 0, 0); g->addWidget(new QPushButton("Cancel"), 0, 1); g->addWidget(off, 0, 2); g->addWidget(tog, 0, 3);
    auto *name = new QLineEdit("Hello"); auto *empty = new QLineEdit; empty->setPlaceholderText("Search…");
    g->addWidget(name, 1, 0, 1, 2); g->addWidget(empty, 1, 2, 1, 2);
    auto *combo = new QComboBox; combo->addItems({"Simple", "Grouped"}); g->addWidget(combo, 2, 0);
    auto *ed = new QComboBox; ed->setEditable(true); ed->addItems({"One", "Two"}); g->addWidget(ed, 2, 1);
    g->addWidget(new QSpinBox, 2, 2);
    auto *tb = new QToolButton; tb->setText("Tool"); g->addWidget(tb, 2, 3);
    auto *c1 = new QCheckBox("Checked"); c1->setChecked(true);
    auto *c2 = new QCheckBox("Plain"); auto *c3 = new QCheckBox("Partly"); c3->setTristate(true); c3->setCheckState(Qt::PartiallyChecked);
    g->addWidget(c1, 3, 0); g->addWidget(c2, 3, 1); g->addWidget(c3, 3, 2);
    auto *r1 = new QRadioButton("On"); r1->setChecked(true); g->addWidget(r1, 4, 0); g->addWidget(new QRadioButton("Off"), 4, 1);
    auto *s = new QSlider(Qt::Horizontal); s->setValue(40); g->addWidget(s, 5, 0, 1, 2);
    auto *pb = new QProgressBar; pb->setValue(65); g->addWidget(pb, 5, 2, 1, 2);
    auto *list = new QListWidget; for (int i = 0; i < 40; i++) list->addItem(QString("Item %1").arg(i)); list->setCurrentRow(1);
    auto *tabs = new QTabWidget; tabs->addTab(list, "List"); tabs->addTab(new QWidget, "Other");
    g->addWidget(tabs, 6, 0, 1, 4);
    auto *menu = new QMenu("Menu");
    menu->addAction(QIcon::fromTheme("document-open"), "No Minecraft profile")->setShortcut(QKeySequence("Ctrl+1"));
    auto *hot = menu->addAction(QIcon::fromTheme("user-identity"), "Supchikwork2"); hot->setShortcut(QKeySequence("Ctrl+2")); hot->setCheckable(true); hot->setChecked(true);
    menu->addSeparator();
    menu->addAction("No default account")->setEnabled(false);
    menu->addMenu("Manage accounts…")->addAction("Add");
    tb->setMenu(menu);
    tb->setPopupMode(QToolButton::InstantPopup);
    ok->setToolTip("Runs the thing\nCtrl+R");
    w.resize(520, 520);
    // DEMO_POPUP=ms: open the tool button's menu after that long and stay, to look at a popup on a desktop
    if (int ms = qEnvironmentVariableIntValue("DEMO_POPUP")) QTimer::singleShot(ms, [tb] { tb->showMenu(); });
    w.show();
    if (argc > 1) {
        QString out = argv[1];
        QTimer::singleShot(300, [&w, out, menu, ok, combo] {
            w.grab().save(out);
            // the popups are windows of their own: a picture of each
            QString base = out.left(out.lastIndexOf('.'));
            menu->popup(QPoint(0, 0));
            QTimer::singleShot(200, [=] { menu->grab().save(base + "-menu.png"); menu->close();
                QToolTip::showText(QPoint(0, 0), "Runs the thing\nCtrl+R", ok);
                QTimer::singleShot(200, [=] { if (auto *t = QApplication::topLevelAt(QPoint(0, 0))) t->grab().save(base + "-tip.png");
                    for (QWidget *x : QApplication::topLevelWidgets()) if (x->inherits("QTipLabel") && x->isVisible()) x->grab().save(base + "-tip.png");
                    QApplication::quit(); }); });
        });
    }
    (void)hot;
    // DEMO_COMBO=file-prefix: open the combo box, move its highlight and take pictures of the popup
    if (qEnvironmentVariableIsSet("DEMO_COMBO")) {
        QString base = qEnvironmentVariable("DEMO_COMBO");
        combo->addItems({"Third", "Fourth"});
        QTimer::singleShot(300, [=] {
            combo->showPopup();
            QTimer::singleShot(250, [=] {
                QWidget *pop = combo->view()->window();
                combo->view()->setCurrentIndex(combo->model()->index(0, 0));
                QTimer::singleShot(250, [=] {
                    combo->view()->setCurrentIndex(combo->model()->index(3, 0));
                    for (int ms : {30, 80, 400}) QTimer::singleShot(ms, [=] { pop->grab().save(QString("%1-%2.png").arg(base).arg(ms)); });
                    QTimer::singleShot(450, [] { QApplication::quit(); });
                });
            });
        });
    }
    // DEMO_LIST=file-prefix: move the chosen row of the list and take pictures on the way
    if (qEnvironmentVariableIsSet("DEMO_LIST")) {
        QString base = qEnvironmentVariable("DEMO_LIST");
        QWidget *wp = &w;
        QTimer::singleShot(300, [=] {
            list->setCurrentRow(1);
            QTimer::singleShot(250, [=] {
                list->setCurrentRow(6);
                for (int ms : {20, 70, 120, 400}) QTimer::singleShot(ms, [=] { wp->grab().save(QString("%1-%2.png").arg(base).arg(ms)); });
                QTimer::singleShot(450, [] { QApplication::quit(); });
            });
        });
    }
    // DEMO_SLIDE=file-prefix: open the menu, move the highlight from the first item to the third and take pictures on the way
    if (qEnvironmentVariableIsSet("DEMO_SLIDE")) {
        QString base = qEnvironmentVariable("DEMO_SLIDE");
        QTimer::singleShot(200, [=] {
            menu->popup(QPoint(40, 40));
            menu->setActiveAction(menu->actions().at(0));
            QTimer::singleShot(250, [=] {
                menu->setActiveAction(menu->actions().at(4));
                int i = 0;
                for (int ms : {20, 60, 110, 400}) QTimer::singleShot(ms, [=] { menu->grab().save(QString("%1-%2.png").arg(base).arg(ms)); });
                QTimer::singleShot(450, [] { QApplication::quit(); });
                (void)i;
            });
        });
    }
    return a.exec();
}
