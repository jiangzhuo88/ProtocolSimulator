#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include "ui/mainwindow.h"

static const char *kAppStyle = R"(
* { font-family: "Microsoft YaHei", "Segoe UI", "PingFang SC", sans-serif; font-size: 9pt; }

QWidget {
    background: #f5f6f8;
    color: #2c3e50;
}

QMainWindow, QDialog { background: #f5f6f8; }

QGroupBox {
    border: 1px solid #dfe4ea;
    border-radius: 6px;
    margin-top: 12px;
    padding-top: 8px;
    background: #ffffff;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 6px;
    color: #5a6c7d;
    font-weight: bold;
}

QPushButton {
    background: #ffffff;
    border: 1px solid #c5ced6;
    border-radius: 5px;
    padding: 5px 14px;
    color: #2c3e50;
}
QPushButton:hover { background: #eaf2fb; border-color: #4a90d9; }
QPushButton:pressed { background: #d0e3f7; }
QPushButton:default { background: #4a90d9; color: white; border-color: #3a7bc8; }
QPushButton:default:hover { background: #5a9fe8; }
QPushButton:disabled { color: #b0b8c0; background: #f0f2f4; border-color: #e0e4e8; }

QToolButton {
    background: #ffffff;
    border: 1px solid #c5ced6;
    border-radius: 4px;
    padding: 3px 8px;
}
QToolButton:hover { background: #eaf2fb; border-color: #4a90d9; }
QToolButton:pressed { background: #d0e3f7; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: #ffffff;
    border: 1px solid #c5ced6;
    border-radius: 4px;
    padding: 3px 6px;
    selection-background-color: #4a90d9;
    selection-color: white;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus,
QPlainTextEdit:focus, QTextEdit:focus {
    border: 1px solid #4a90d9;
}
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: #ffffff;
    border: 1px solid #c5ced6;
    selection-background-color: #4a90d9;
    selection-color: white;
    outline: none;
}
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    background: #f0f2f4; border: none; width: 16px;
}

QTableWidget {
    background: #ffffff;
    alternate-background-color: #f7f9fb;
    border: 1px solid #dfe4ea;
    border-radius: 4px;
    gridline-color: #eaeef2;
    selection-background-color: #cfe3f7;
    selection-color: #2c3e50;
}
QHeaderView::section {
    background: #eef1f5;
    color: #5a6c7d;
    padding: 5px;
    border: none;
    border-right: 1px solid #dfe4ea;
    border-bottom: 1px solid #dfe4ea;
    font-weight: bold;
}
QTableCornerButton::section { background: #eef1f5; border: none; }

QListWidget {
    background: #ffffff;
    border: 1px solid #dfe4ea;
    border-radius: 4px;
    outline: none;
}
QListWidget::item { padding: 4px 6px; border-radius: 3px; }
QListWidget::item:hover { background: #eaf2fb; }
QListWidget::item:selected { background: #4a90d9; color: white; }

QCheckBox { spacing: 6px; }
QCheckBox::indicator { width: 15px; height: 15px; border-radius: 3px; }
QCheckBox::indicator:unchecked { background: #ffffff; border: 1px solid #c5ced6; }
QCheckBox::indicator:checked { background: #4a90d9; border: 1px solid #3a7bc8; image: none; }

QLabel { background: transparent; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #c5ced6; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #4a90d9; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #c5ced6; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: #4a90d9; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QTabWidget::pane { border: 1px solid #dfe4ea; border-radius: 4px; top: -1px; }
QTabBar::tab {
    background: #eef1f5; color: #5a6c7d;
    padding: 6px 16px; border: 1px solid #dfe4ea;
    border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px;
    margin-right: 2px;
}
QTabBar::tab:selected { background: #ffffff; color: #2c3e50; }
QTabBar::tab:hover:!selected { background: #e3e8ee; }

QMenu { background: #ffffff; border: 1px solid #dfe4ea; }
QMenu::item { padding: 6px 24px; }
QMenu::item:selected { background: #4a90d9; color: white; }

QToolTip { background: #2c3e50; color: #ffffff; border: none; padding: 4px 8px; border-radius: 3px; }
)";

// ===== 国际化: 全局翻译器管理 =====
static QTranslator *g_QtTranslator  = nullptr; // Qt自带控件翻译(OK/取消/文件对话框等)
QTranslator       *g_appTranslator = nullptr; // ProtocolSimulator应用级翻译
static QString    g_currentLang    = "zh";    // "zh" | "en"

static QString s_langSettingFile()
{
    // 简单保存在应用目录的 .lang 文本, 不依赖独立settings
    return QApplication::applicationDirPath() + "/.language";
}

static bool s_loadLangFileIfExists(QTranslator *t, const QString &lang)
{
    // 1) 内嵌资源 :/translations/ProtocolSimulator_xx.qm
    const QString res = QString(":/translations/ProtocolSimulator_%1.qm").arg(lang);
    if (QFile::exists(res) && t->load(res)) return true;
    // 2) 可执行文件目录 translations/ProtocolSimulator_xx.qm (方便用户替换, 不重编译)
    const QString disk = QApplication::applicationDirPath()
                       + QString("/translations/ProtocolSimulator_%1.qm").arg(lang);
    if (QFile::exists(disk) && t->load(disk)) return true;
    return false;
}

// 在MainWindow里由菜单调用: zh/en
void switchAppLanguage(const QString &langCode)
{
    QString code = (langCode == "en") ? "en" : "zh";
    if (code == g_currentLang && g_appTranslator) return;

    QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) return;

    // --- 卸载已装的翻译 ---
    if (g_appTranslator) {
        app->removeTranslator(g_appTranslator);
        delete g_appTranslator;
        g_appTranslator = nullptr;
    }
    if (g_QtTranslator) {
        app->removeTranslator(g_QtTranslator);
        delete g_QtTranslator;
        g_QtTranslator = nullptr;
    }

    g_currentLang = code;
    // 保存选择(下次启动自动切)
    QFile f(s_langSettingFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(code.toUtf8());
        f.close();
    }

    if (code == "en") {
        // 应用翻译
        g_appTranslator = new QTranslator(app);
        s_loadLangFileIfExists(g_appTranslator, "en");
        app->installTranslator(g_appTranslator);

        // Qt基础对话框翻译: 可选地加载Qt自己的en翻译(默认系统已处理, 但资源内嵌qt_en.qm也可)
        // 这里只留指针, 不加载默认英文也能用
        g_QtTranslator = new QTranslator(app);
        const QString qtRes = QString(":/translations/qtbase_%1.qm").arg(code);
        if (QFile::exists(qtRes) && g_QtTranslator->load(qtRes))
            app->installTranslator(g_QtTranslator);
    }
    // zh: 不装自定义翻译 -> 走源代码中的中文原文(源字符串就是中文, zero-copy)
}

// 获取当前语言代码: "zh" | "en" —— 供MainWindow更新语言菜单勾选状态用
QString currentAppLanguage()
{
    return g_currentLang;
}

static QString s_initialLangChoice()
{
    QFile f(s_langSettingFile());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString saved = QString::fromUtf8(f.readAll()).trimmed().toLower();
        f.close();
        if (saved == "en" || saved == "zh") return saved;
    }
    // 没有保存过, 就按系统语言: 非中文系统 → 默认英文
    const QString sysLang = QLocale::system().name().toLower();  // "zh_cn", "en_us", ...
    if (sysLang.startsWith("zh")) return "zh";
    return "en";
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion"));
    a.setStyleSheet(QString::fromLatin1(kAppStyle));
    a.setApplicationName("ProtocolSimulator");

    // 启动时用保存语言或系统语言, 先切换一次(卸载/安装流程都是统一的)
    switchAppLanguage(s_initialLangChoice());

    MainWindow w;
    w.show();

    return a.exec();
}
