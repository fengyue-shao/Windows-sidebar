#include "include/terminalscreen.h"
#include <QCoreApplication>
#include <QDebug>
#include <iostream>
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL " << __LINE__ << ": " << #c << '\n'; return 1; } } while (false)
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    TerminalScreen screen;
    screen.resize(20, 4);
    const QByteArray chinese = QStringLiteral("中文🙂").toUtf8();
    for (char byte : chinese) screen.feed(QByteArray(1, byte));
    CHECK(screen.plainText() == QStringLiteral("中文🙂"));
    CHECK(screen.cursorColumn() == 6);
    screen.feed("\rOK\x1b[K");
    CHECK(screen.plainText() == "OK");
    screen.feed("\x1b[2;4H\x1b[1;31m<&>\x1b[0m");
    CHECK(screen.cursorRow() == 1 && screen.cursorColumn() == 6);
    CHECK(screen.renderedLines()[1].contains("&lt;&amp;&gt;"));
    CHECK(screen.renderedLines()[1].contains("#ef6b73"));
    CHECK(screen.renderedLines()[1].contains("font-weight:bold"));
    screen.feed("\x1b[38;2;12;34;56mX\x1b[48;5;240mY");
    CHECK(screen.renderedLines()[1].contains("#0c2238"));
    CHECK(screen.renderedLines()[1].contains("background-color:#585858"));
    screen.clear();
    screen.feed("primary\x1b[?1049halternate\x1b[?25l\x1b[?2004h\x1b[?1h");
    CHECK(screen.alternateScreen() && screen.plainText() == "alternate");
    CHECK(!screen.cursorVisible() && screen.bracketedPaste() && screen.applicationCursorKeys());
    screen.feed("\x1b[?1049l");
    CHECK(!screen.alternateScreen() && screen.plainText() == "primary" && screen.cursorColumn() == 7);
    screen.clear();
    CHECK(screen.bracketedPaste() && screen.applicationCursorKeys() && !screen.cursorVisible());
    screen.resetForSession();
    CHECK(!screen.bracketedPaste() && !screen.applicationCursorKeys() && screen.cursorVisible());
    for (int i = 0; i < 2100; ++i) screen.feed(QByteArray::number(i) + "\r\n");
    CHECK(screen.scrollbackRows() == 2000 && screen.renderedLines().size() == 2004);
    CHECK(screen.cursorRow() == 2003);
    screen.feed("\x1b[3J");
    CHECK(screen.scrollbackRows() == 0);
    screen.clear();
    screen.feed("\x1b[2;3r\x1b[2;1HX\r\nY\r\nZ");
    CHECK(screen.scrollbackRows() == 0);
    CHECK(screen.plainText() == "\nY\nZ");
    screen.clear();
    screen.feed("abcde\x1b[1;2H\x1b[2P");
    CHECK(screen.plainText() == "ade");
    screen.feed("\x1b[2@XY");
    CHECK(screen.plainText() == "aXYde");
    screen.clear();
    screen.feed("e"); screen.feed(QStringLiteral("\u0301").toUtf8());
    CHECK(screen.cursorColumn() == 1 && screen.plainText() == QStringLiteral("e\u0301"));
    QStringList replies;
    QObject::connect(&screen, &TerminalScreen::responseRequested, &app, [&](const QString &reply) { replies.append(reply); });
    screen.feed("\x1b[6n\x1b[5n\x1b[c");
    CHECK(replies == QStringList({"\x1b[1;2R", "\x1b[0n", "\x1b[?1;2c"}));
    screen.feed("\x1b]2;fixture title\x1b\\\x1b]52;c;ignored\x07\x1bPignored data\x1b\\");
    CHECK(screen.title() == "fixture title" && !screen.plainText().contains("ignored"));
    screen.feed("\x1b]" + QByteArray(100000, 'a') + "\x07tail");
    CHECK(screen.plainText().endsWith("tail"));
    screen.resize(999, 999);
    CHECK(screen.columns() == 240 && screen.rows() == 100);
    screen.clear();
    screen.resize(20, 4);
    screen.feed(QByteArray(20, 'a') + "b");
    CHECK(screen.cursorRow() == 1 && screen.cursorColumn() == 1);
    CHECK(screen.plainText() == QByteArray(20, 'a') + "\nb");
    qInfo() << "PASS incremental UTF8 + CJK + VT colors/cursor/erase + alternate screen + bounded history + queries + control filtering";
    return 0;
}
