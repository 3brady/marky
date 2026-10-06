#include "app/Theme.h"
#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>
#include <QtGlobal>

namespace theme
{

    bool systemPrefersDark()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        return false;
#endif
    }

    static QPalette darkPalette()
    {
        const QColor window(45, 45, 48), base(30, 30, 30), text(220, 220, 220);

        QPalette p;
        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, window);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, window);
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::ToolTipBase, window);
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::PlaceholderText, QColor(140, 140, 140));
        p.setColor(QPalette::Highlight, QColor(42, 130, 218));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::Link, QColor(95, 170, 255));
        p.setColor(QPalette::Dark, QColor(110, 110, 115));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
        p.setColor(QPalette::Mid, QColor(80, 80, 85));
        p.setColor(QPalette::Midlight, QColor(60, 60, 64));
        p.setColor(QPalette::Light, QColor(70, 70, 75));
        p.setColor(QPalette::Shadow, QColor(15, 15, 15));
        return p;
    }

    void apply(bool dark)
    {
        QApplication::setPalette(dark ? darkPalette() : QApplication::style()->standardPalette());
    }

}