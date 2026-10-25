#pragma once
#include <QtWidgets>
namespace LedgerTheme
{
inline void apply(bool dark)
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#efe3ca"));
    palette.setColor(QPalette::WindowText, QColor("#33291f"));
    palette.setColor(QPalette::Base, QColor("#fff8e8"));
    palette.setColor(QPalette::AlternateBase, QColor("#eee0c2"));
    palette.setColor(QPalette::Text, QColor("#33291f"));
    palette.setColor(QPalette::Button, QColor("#e3d0a9"));
    palette.setColor(QPalette::ButtonText, QColor("#33291f"));
    palette.setColor(QPalette::Highlight, QColor("#75483a"));
    palette.setColor(QPalette::HighlightedText, QColor("#fff6df"));

    QString style = R"(
      QMainWindow,QScrollArea,QTabWidget::pane {background:#efe3ca;}
      QWidget#sidebar {background:#302a24;color:#f2e4c7;} QWidget#sidebar QLabel {color:#e4d1a7;}
      QLabel#brand {font-family:Georgia;font-size:23px;font-weight:bold;color:#f1ddb4;letter-spacing:2px;}
      QLabel#summary {font-family:Georgia;font-size:20px;color:#63352e;padding-bottom:8px;}
      QLabel#muted {color:#7c6c54;font-size:11px;}
      QPushButton {background:#e4d2ad;border:1px solid #b9a178;border-radius:4px;padding:7px 12px;}
      QPushButton:hover {background:#d9c296;border-color:#885c3c;} QPushButton:pressed {background:#c7a979;}
      QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QPlainTextEdit {background:#fff8e9;color:#33291f;border:1px solid #c3ae89;border-radius:3px;padding:5px;selection-background-color:#75483a;}
      QGroupBox {font-weight:bold;border:1px solid #b8a17e;border-radius:4px;margin-top:14px;padding:15px 10px 10px;}
      QGroupBox::title {subcontrol-origin:margin;left:12px;padding:0 6px;color:#79463a;}
      QTabWidget::pane {border:1px solid #c3ae89;} QTabBar::tab {background:#dcc8a5;border:1px solid #bba47d;padding:8px 10px;color:#47372b;}
      QTabBar::tab:selected {background:#fff6e2;color:#713a30;border-bottom-color:#fff6e2;}
      QListWidget {background:#302a24;border:0;color:#f1e1bf;outline:0;} QListWidget::item {padding:10px 6px;border-bottom:1px solid #494033;} QListWidget::item:selected {background:#69523c;}
      QTableWidget {background:#fff7e5;border:1px solid #c4af87;gridline-color:#ddcbaa;selection-background-color:#80604a;selection-color:#fff7e5;}
      QHeaderView::section {background:#ddc8a1;color:#49372a;border:0;border-bottom:1px solid #bda67c;padding:7px;}
      QMenuBar,QMenu {background:#e2cfab;color:#382d23;} QMenu::item:selected {background:#bba17b;}
      QToolTip {background:#fff8e8;color:#33291f;border:1px solid #bba47d;}
    )";

    if (dark)
    {
        palette.setColor(QPalette::Window, QColor("#202327"));
        palette.setColor(QPalette::WindowText, QColor("#e7e3da"));
        palette.setColor(QPalette::Base, QColor("#171a1e"));
        palette.setColor(QPalette::AlternateBase, QColor("#282c32"));
        palette.setColor(QPalette::Text, QColor("#e7e3da"));
        palette.setColor(QPalette::Button, QColor("#30353b"));
        palette.setColor(QPalette::ButtonText, QColor("#e7e3da"));
        palette.setColor(QPalette::Highlight, QColor("#675438"));
        palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        palette.setColor(QPalette::Link, QColor("#e1b975"));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#959ca5"));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#959ca5"));
        const QMap<QString, QString> colors = {
            {"#efe3ca", "#202327"}, {"#33291f", "#e7e3da"}, {"#fff8e8", "#171a1e"},
            {"#302a24", "#171a1e"}, {"#f2e4c7", "#e7e3da"}, {"#e4d1a7", "#cbc6bc"},
            {"#f1ddb4", "#e1b975"}, {"#63352e", "#e1b975"}, {"#7c6c54", "#aab0b8"},
            {"#e4d2ad", "#30353b"}, {"#b9a178", "#515760"}, {"#d9c296", "#414750"},
            {"#885c3c", "#c09b60"}, {"#c7a979", "#4b4335"}, {"#fff8e9", "#171a1e"},
            {"#c3ae89", "#515760"}, {"#75483a", "#675438"}, {"#b8a17e", "#515760"},
            {"#79463a", "#e1b975"}, {"#dcc8a5", "#30353b"}, {"#bba47d", "#515760"},
            {"#47372b", "#d4d0c8"}, {"#fff6e2", "#3a414a"}, {"#713a30", "#f0d39a"},
            {"#f1e1bf", "#e7e3da"}, {"#494033", "#343941"}, {"#69523c", "#675438"},
            {"#fff7e5", "#171a1e"}, {"#c4af87", "#515760"}, {"#ddcbaa", "#3f464f"},
            {"#80604a", "#675438"}, {"#ddc8a1", "#30353b"}, {"#49372a", "#e7e3da"},
            {"#bda67c", "#515760"}, {"#e2cfab", "#282c32"}, {"#382d23", "#e7e3da"},
            {"#bba17b", "#675438"}};
        QRegularExpression pattern("#[a-fA-F0-9]{6}");
        QString converted;
        int start = 0;
        auto matches = pattern.globalMatch(style);
        while (matches.hasNext())
        {
            auto match = matches.next();
            converted += style.mid(start, match.capturedStart() - start);
            converted += colors.value(match.captured(), match.captured());
            start = match.capturedEnd();
        }
        style = converted + style.mid(start);
        style += "QPushButton:disabled,QLineEdit:disabled,QComboBox:disabled {color:#959ca5;} "
                 "QTableWidget {selection-color:#ffffff;}";
    }
    qApp->setPalette(palette);
    qApp->setStyleSheet(style);
    for (auto *widget : QApplication::allWidgets())
        widget->update();
}
} // namespace LedgerTheme
