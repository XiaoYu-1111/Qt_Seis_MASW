#pragma once
#include <QString>

namespace StyleHelper {
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