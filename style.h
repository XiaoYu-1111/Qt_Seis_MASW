#pragma once
#include <QString>
#include <QPalette>

namespace StyleHelper {
    enum class ThemeMode { LightSage, Lavender, NordicBlue, SummerMeadow, ClassicWhite, DeepSlate, Mint, Rose, WarmSand, Twilight };

    struct ThemeColors {
        QString bgApp, bgCard, bgHover, bgHeader, border, focus;
        QString text, textSecondary, textMuted, primary, accent, buttonText;
    };

    inline ThemeColors themeColors(ThemeMode mode) {
        switch (mode) {
        case ThemeMode::Lavender: return {"#f5f3f8", "#ffffff", "#faf8fc", "#ece7f2", "#dfd5ea", "#6b3ba7", "#2a183d", "#6e5d80", "#9f90b3", "#6b3ba7", "#9568af", "#ffffff"};
        case ThemeMode::NordicBlue: return {"#edf4f8", "#ffffff", "#f5f9fc", "#e1edf5", "#cde1ee", "#026896", "#0e2938", "#476b80", "#7f9fb3", "#026896", "#38a3d8", "#ffffff"};
        case ThemeMode::SummerMeadow: return {"#faf7ee", "#ffffff", "#fdfcf7", "#f3edd8", "#e8dfc5", "#467a57", "#2a241b", "#695f51", "#9e9383", "#467a57", "#f28e00", "#ffffff"};
        case ThemeMode::ClassicWhite: return {"#f4f5f7", "#ffffff", "#f0f2f5", "#e7e9ed", "#c8cdd4", "#0072bd", "#222222", "#4b5563", "#6b7280", "#0072bd", "#4dbeee", "#ffffff"};
        case ThemeMode::DeepSlate: return {"#101c2e", "#202d42", "#263852", "#18263b", "#2d3e57", "#6b8fbd", "#edf3fc", "#a8bbd8", "#7489a8", "#4b6b98", "#7294c2", "#ffffff"};
        case ThemeMode::Mint: return {"#edf7f4", "#ffffff", "#f5fbf9", "#dcefe9", "#c8e2d9", "#21866d", "#18352e", "#4d766a", "#83a69b", "#21866d", "#43a889", "#ffffff"};
        case ThemeMode::Rose: return {"#faf2f4", "#ffffff", "#fdf8f9", "#f2e3e8", "#e8d1da", "#a34d6d", "#38212b", "#765767", "#a58a97", "#a34d6d", "#c47491", "#ffffff"};
        case ThemeMode::WarmSand: return {"#f8f4eb", "#fffdf8", "#fcf9f2", "#eee5d3", "#dfd2b9", "#96713b", "#352d21", "#76664d", "#a99b84", "#96713b", "#c18b42", "#ffffff"};
        case ThemeMode::Twilight: return {"#f1f2f9", "#ffffff", "#f8f8fc", "#e4e5f1", "#d1d3e5", "#555f9c", "#24263d", "#5c607c", "#888ba5", "#555f9c", "#7b83c0", "#ffffff"};
        case ThemeMode::LightSage:
        default: return {"#eef3eb", "#ffffff", "#f7faf5", "#e5ede2", "#d8e3d3", "#3a5a40", "#1a2e1d", "#52796f", "#84a98c", "#344e41", "#588157", "#ffffff"};
        }
    }

    inline QString themeName(ThemeMode mode) {
        switch (mode) {
        case ThemeMode::Lavender: return QStringLiteral("优雅薰衣草");
        case ThemeMode::NordicBlue: return QStringLiteral("冰川静蓝");
        case ThemeMode::SummerMeadow: return QStringLiteral("夏日草甸");
        case ThemeMode::ClassicWhite: return QStringLiteral("经典白色 (MATLAB)");
        case ThemeMode::DeepSlate: return QStringLiteral("深邃蓝灰");
        case ThemeMode::Mint: return QStringLiteral("海盐薄荷");
        case ThemeMode::Rose: return QStringLiteral("樱雾玫瑰");
        case ThemeMode::WarmSand: return QStringLiteral("暖阳砂岩");
        case ThemeMode::Twilight: return QStringLiteral("暮色靛蓝");
        default: return QStringLiteral("清爽米绿");
        }
    }

    inline QString getThemeStyle(ThemeMode mode) {
        const auto c = themeColors(mode);
        return QString(R"(
            QWidget { background-color:%1; color:%2; font-family:'Segoe UI','Microsoft YaHei',sans-serif; font-size:10pt; }
            QMainWindow,QDialog { background-color:%1; }
            QMenuBar,QToolBar,QStatusBar { background-color:%4; color:%2; border:0; }
            QMenu { background-color:%3; color:%2; border:1px solid %5; padding:4px; }
            QMenu::item { padding:6px 22px; } QMenu::item:selected { background-color:%10; color:%11; }
            QGroupBox { border:1px solid %5; border-radius:6px; margin-top:18px; padding-top:8px; font-weight:600; }
            QGroupBox::title { color:%10; subcontrol-origin:margin; left:10px; padding:0 4px; }
            QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QTextEdit,QPlainTextEdit { background-color:%3; color:%2; border:1px solid %5; border-radius:4px; padding:4px 7px; selection-background-color:%10; }
            QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus,QTextEdit:focus { border:1px solid %6; }
            QPushButton,QToolButton { background-color:%4; color:%2; border:1px solid %5; border-radius:5px; padding:6px 12px; }
            QPushButton:hover,QToolButton:hover { background-color:%7; } QPushButton:pressed,QToolButton:pressed { background-color:%3; }
            QPushButton#btnPrimary { background-color:%10; color:%11; border-color:%6; font-weight:bold; }
            QPushButton#btnPrimary:hover { background-color:%6; }
            QPushButton#btnPrimary:pressed { background-color:%10; }
            QPushButton#btnPrimary:disabled { background-color:%4; color:%9; border-color:%5; }
            QTabWidget::pane { background-color:%3; border:1px solid %5; }
            QTabBar::tab { background-color:%4; color:%8; padding:8px 16px; margin-right:2px; }
            QTabBar::tab:selected { background-color:%3; color:%10; border-bottom:2px solid %10; }
            QLabel#sectionInfo { background-color:%7; color:%2; border:1px solid %5; border-left:4px solid %10; border-radius:6px; padding:9px 12px; font-weight:600; }
            QDockWidget { color:%2; } QDockWidget::title { background:%4; padding:6px; border-bottom:1px solid %5; }
            QMainWindow::separator { background:%5; width:3px; height:3px; }
            QScrollBar:vertical { background:%1; width:9px; } QScrollBar::handle:vertical { background:%5; min-height:20px; border-radius:4px; }
        )").arg(c.bgApp, c.text, c.bgCard, c.bgHeader, c.border, c.focus, c.bgHover, c.textSecondary, c.textMuted, c.primary, c.buttonText);
    }

    // 现代深色地学分析风格主题 (Slate Deep Blue Theme)
    inline QString getDarkScientificStyle() {
        return R"(
        /* 全局基础设置 */
        QWidget {
            background-color: #1e293b;
            color: #cbd5e1;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
            font-size: 10pt;
        }

        /* 主窗口与对话框 */
        QMainWindow, QDialog {
            background-color: #0f172a;
        }

        /* 菜单栏与工具栏 */
        QMenuBar {
            background-color: #1e293b;
            color: #e2e8f0;
            border-bottom: 1px solid #334155;
        }
        QMenuBar::item:selected {
            background-color: #334155;
            border-radius: 4px;
        }
        QMenu {
            background-color: #1e293b;
            color: #cbd5e1;
            border: 1px solid #475569;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px;
            border-radius: 3px;
        }
        QMenu::item:selected {
            background-color: #0284c7;
            color: #ffffff;
        }
        QToolBar {
            background-color: #1e293b;
            border-bottom: 1px solid #334155;
            spacing: 6px;
            padding: 4px;
        }

        /* 分组框 (GroupBox) */
        QGroupBox {
            font-weight: bold;
            border: 1px solid #334155;
            border-radius: 6px;
            margin-top: 24px;
            padding-top: 10px;
            background-color: #1e293b;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
            color: #38bdf8; /* 突出亮蓝 */
        }

        /* 输入控件 (LineEdit, SpinBox, ComboBox) */
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #0f172a;
            border: 1px solid #475569;
            border-radius: 4px;
            padding: 4px 8px;
            color: #f1f5f9;
            selection-background-color: #0284c7;
            min-height: 22px;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid #38bdf8;
            background-color: #172554;
        }

        /* 下拉框微调 */
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: none;
        }

        /* 按钮通用 */
        QPushButton {
            background-color: #334155;
            border: 1px solid #475569;
            color: #f1f5f9;
            border-radius: 4px;
            padding: 6px 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #475569;
            border-color: #64748b;
        }
        QPushButton:pressed {
            background-color: #1e293b;
        }
        QPushButton:disabled {
            background-color: #1e293b;
            border-color: #334155;
            color: #64748b;
        }

        /* 主强调按钮（如：开始计算） */
        QPushButton#btnPrimary {
            background-color: #0284c7;
            border: 1px solid #0369a1;
            color: #ffffff;
            font-weight: bold;
        }
        QPushButton#btnPrimary:hover {
            background-color: #0369a1;
        }

        /* 选项卡 (TabWidget) */
        QTabWidget::pane {
            border: 1px solid #334155;
            background-color: #1e293b;
            border-radius: 0 0 6px 6px;
        }
        QTabBar::tab {
            background: #0f172a;
            color: #94a3b8;
            padding: 8px 18px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            margin-right: 3px;
        }
        QTabBar::tab:selected {
            background: #1e293b;
            color: #38bdf8;
            font-weight: bold;
            border-top: 2px solid #38bdf8;
        }
        QTabBar::tab:hover:!selected {
            background: #1e293b;
            color: #e2e8f0;
        }

        /* 停靠窗 (DockWidget) */
        QDockWidget {
            color: #e2e8f0;
            font-weight: bold;
        }
        QDockWidget::title {
            background-color: #0f172a;
            border-bottom: 1px solid #334155;
            padding: 6px;
            text-align: left;
        }

        /* 状态栏 */
        QStatusBar {
            background-color: #0f172a;
            color: #94a3b8;
            border-top: 1px solid #334155;
        }

        /* 滚动条美化 */
        QScrollBar:vertical {
            background: #0f172a;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #334155;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #475569;
        }
/* 停靠窗 (DockWidget) */
        QDockWidget {
            color: #e2e8f0;
            font-weight: bold;
            border: 1px solid #1e293b; /* 边框用暗色，防止发白 */
        }
        QDockWidget::title {
            background-color: #0f172a;
            border-bottom: 1px solid #334155;
            padding: 6px;
            text-align: left;
        }

        /* 核心修复：主窗口与 Dock 之间的分割条（彻底消除发白线条） */
        QMainWindow::separator {
            background-color: #1e293b; /* 与周围背景同色或深色，绝不发白 */
            width: 2px;
            height: 2px;
            margin: 0;
            padding: 0;
        }
        QMainWindow::separator:hover {
            background-color: #0284c7; /* 鼠标悬停可拖拽时显示高亮蓝 */
        }
        )";
    }
}
