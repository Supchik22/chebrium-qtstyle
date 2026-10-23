// Everything else a Qt program has, in one window, to look at how the style draws it (offscreen: more <file.png>)
#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>

int main(int argc, char **argv) {
    QApplication a(argc, argv);
    QMainWindow w;
    w.setWindowTitle("more");
    auto *mb = w.menuBar();
    mb->addMenu("File")->addAction("Open");
    mb->addMenu("Edit")->addAction("Copy");
    mb->addMenu("Help")->addAction("About");
    auto *tb = w.addToolBar("main");
    tb->addAction(QIcon::fromTheme("document-open"), "Open");
    tb->addAction(QIcon::fromTheme("document-save"), "Save")->setCheckable(true);
    tb->addSeparator();
    auto *act = tb->addAction(QIcon::fromTheme("edit-copy"), "Copy");
    act->setEnabled(false);
    tb->addWidget(new QLineEdit("search"));
    auto *tbtn = new QToolButton; tbtn->setText("Menu"); tbtn->setPopupMode(QToolButton::MenuButtonPopup); tbtn->setMenu(new QMenu); tb->addWidget(tbtn);

    auto *split = new QSplitter;
    auto *tree = new QTreeView;
    auto *model = new QStandardItemModel;
    model->setHorizontalHeaderLabels({"Name", "Size", "Kind"});
    for (int i = 0; i < 4; i++) {
        auto *p = new QStandardItem(QString("Folder %1").arg(i));
        for (int j = 0; j < 3; j++) p->appendRow({new QStandardItem(QString("file %1.%2").arg(i).arg(j)), new QStandardItem("12 KB"), new QStandardItem("text")});
        model->appendRow({p, new QStandardItem("--"), new QStandardItem("folder")});
    }
    tree->setModel(model);
    tree->expandAll();
    tree->setAlternatingRowColors(true);
    tree->setCurrentIndex(model->index(1, 0));
    auto *table = new QTableWidget(5, 3);
    table->setHorizontalHeaderLabels({"A", "B", "C"});
    for (int r = 0; r < 5; r++) for (int c = 0; c < 3; c++) table->setItem(r, c, new QTableWidgetItem(QString("%1,%2").arg(r).arg(c)));
    table->setCurrentCell(2, 1);
    auto *tabs = new QTabWidget;
    tabs->addTab(table, "Table");
    tabs->addTab(new QPlainTextEdit("plain text\nsecond line"), "Text");
    tabs->setTabsClosable(true);
    split->addWidget(tree);
    split->addWidget(tabs);
    auto *center = new QWidget;
    auto *lay = new QVBoxLayout(center);
    lay->addWidget(split, 1);
    auto *gb = new QGroupBox("Options");
    auto *gl = new QVBoxLayout(gb);
    auto *row = new QWidget; auto *rl = new QHBoxLayout(row); rl->setContentsMargins(0, 0, 0, 0);
    rl->addWidget(new QCheckBox("One")); rl->addWidget(new QRadioButton("Two")); rl->addWidget(new QSpinBox); auto *cb = new QComboBox; cb->addItems({"x", "y"}); rl->addWidget(cb);
    gl->addWidget(row);
    auto *sl = new QSlider(Qt::Horizontal); sl->setValue(30); gl->addWidget(sl);
    auto *pb = new QProgressBar; pb->setValue(55); gl->addWidget(pb);
    lay->addWidget(gb);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help);
    lay->addWidget(bb);
    w.setCentralWidget(center);
    auto *dock = new QDockWidget("Dock");
    dock->setWidget(new QLabel("  a dock widget  "));
    w.addDockWidget(Qt::RightDockWidgetArea, dock);
    w.statusBar()->showMessage("Ready");
    w.statusBar()->addPermanentWidget(new QLabel("42%"));
    w.resize(900, 640);
    w.show();
    // MORE_GLIDE=file-prefix: the pointer enters the first tool button, then the fourth one: pictures of the highlight on the way
    if (qEnvironmentVariableIsSet("MORE_GLIDE")) {
        const QString base = qEnvironmentVariable("MORE_GLIDE");
        auto *first = tb->widgetForAction(tb->actions().at(0)), *fourth = tb->widgetForAction(tb->actions().at(1));
        if (!fourth) fourth = tb->widgetForAction(tb->actions().at(2));
        QTimer::singleShot(300, [=, &w] {
            QEvent in(QEvent::Enter);
            QCoreApplication::sendEvent(first, &in);
            QTimer::singleShot(250, [=, &w] {
                QEvent out(QEvent::Leave);
                QCoreApplication::sendEvent(first, &out);
                QEvent in2(QEvent::Enter);
                QCoreApplication::sendEvent(fourth, &in2);
                for (int ms : {30, 70, 400}) QTimer::singleShot(ms, [=, &w] { w.grab().save(QString("%1-%2.png").arg(base).arg(ms)); });
                QTimer::singleShot(450, [] { QApplication::quit(); });
            });
        });
        return a.exec();
    }
    QString out = argc > 1 ? argv[1] : "more.png";
    QTimer::singleShot(400, [&] { w.grab().save(out); QApplication::quit(); });
    return a.exec();
}
