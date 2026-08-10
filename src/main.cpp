#include <QApplication>
#include <QFile>
#include <QStyleFactory>
#include "ui/mainwindow.h"

// 现代扁平化主题: 浅色背景/圆角/柔和蓝灰主色/hover反馈
// 替代原Windows经典风格, 提升整体观感
static const char *kAppStyle = R"(
* { font-family: "Microsoft YaHei", "Segoe UI", "PingFang SC", sans-serif; font-size: 9pt; }

QWidget {
    background: #f5f6f8;
    color: #2c3e50;
}

/* 主窗口/对话框背景 */
QMainWindow, QDialog { background: #f5f6f8; }

/* 分组框 */
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

/* 按钮: 扁平圆角 + hover/pressed */
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

/* 工具按钮(文件选择"..."等) */
QToolButton {
    background: #ffffff;
    border: 1px solid #c5ced6;
    border-radius: 4px;
    padding: 3px 8px;
}
QToolButton:hover { background: #eaf2fb; border-color: #4a90d9; }
QToolButton:pressed { background: #d0e3f7; }

/* 输入框/下拉框/数字框: 圆角白底 + focus高亮 */
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

/* 表格: 交替行色 + 圆角表头 */
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

/* 列表 */
QListWidget {
    background: #ffffff;
    border: 1px solid #dfe4ea;
    border-radius: 4px;
    outline: none;
}
QListWidget::item { padding: 4px 6px; border-radius: 3px; }
QListWidget::item:hover { background: #eaf2fb; }
QListWidget::item:selected { background: #4a90d9; color: white; }

/* 复选框 */
QCheckBox { spacing: 6px; }
QCheckBox::indicator { width: 15px; height: 15px; border-radius: 3px; }
QCheckBox::indicator:unchecked { background: #ffffff; border: 1px solid #c5ced6; }
QCheckBox::indicator:checked { background: #4a90d9; border: 1px solid #3a7bc8; image: none; }

/* 标签 */
QLabel { background: transparent; }

/* 滚动条: 扁平细条 */
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #c5ced6; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #4a90d9; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #c5ced6; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: #4a90d9; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

/* 选项卡 */
QTabWidget::pane { border: 1px solid #dfe4ea; border-radius: 4px; top: -1px; }
QTabBar::tab {
    background: #eef1f5; color: #5a6c7d;
    padding: 6px 16px; border: 1px solid #dfe4ea;
    border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px;
    margin-right: 2px;
}
QTabBar::tab:selected { background: #ffffff; color: #2c3e50; }
QTabBar::tab:hover:!selected { background: #e3e8ee; }

/* 菜单 */
QMenu { background: #ffffff; border: 1px solid #dfe4ea; }
QMenu::item { padding: 6px 24px; }
QMenu::item:selected { background: #4a90d9; color: white; }

/* 提示 */
QToolTip { background: #2c3e50; color: #ffffff; border: none; padding: 4px 8px; border-radius: 3px; }
)";

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Fusion风格: 跨平台一致的现代外观(优于Windows经典灰)
    a.setStyle(QStyleFactory::create("Fusion"));
    a.setStyleSheet(QString::fromLatin1(kAppStyle));

    MainWindow w;
    w.show();

    return a.exec();
}
