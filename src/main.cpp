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
enum Language {
    Chinese  = 0,
    English  = 1
};
QTranslator       *g_appTranslator = nullptr; // ProtocolSimulator应用级翻译
static Language    g_currentLang    = Chinese;    // "zh" | "en"
// 在MainWindow里由菜单调用: zh/en
void switchAppLanguage(const Language &lang)
{
    if (lang == g_currentLang) return;

    // 移除旧的翻译器
    if (g_appTranslator) {
        qApp->removeTranslator(g_appTranslator);
        delete g_appTranslator;
        g_appTranslator = nullptr;
    }

    // 英文模式：加载 .qm 翻译文件
    if (lang == English) {
        g_appTranslator = new QTranslator();
        // 优先从 Qt 资源系统加载
        bool loaded = g_appTranslator->load(":/translations/ProtocolSimulator_en.qm");
        if (!loaded) {
            // 回退：从可执行文件同目录加载
            QString appDir = QCoreApplication::applicationDirPath();
            loaded = g_appTranslator->load("ProtocolSimulator_en.qm", appDir + "/translations");
        }
        if (loaded) {
            qApp->installTranslator(g_appTranslator);
        }
    }
    // 中文模式：不加载翻译器，直接使用源字符串

    g_currentLang = lang;
}
// 获取当前语言代码: "zh" | "en" —— 供MainWindow更新语言菜单勾选状态用
Language currentAppLanguage()
{
    return g_currentLang;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion"));
    a.setStyleSheet(QString::fromLatin1(kAppStyle));
    a.setApplicationName("ProtocolSimulator");

    // 启动时用保存语言或系统语言, 先切换一次(卸载/安装流程都是统一的)
    switchAppLanguage(English);

    MainWindow w;
    w.show();

    return a.exec();
}
