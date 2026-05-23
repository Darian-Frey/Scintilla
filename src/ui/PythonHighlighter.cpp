#include "PythonHighlighter.h"

namespace {
    // Block state values for tracking multi-line triple-quoted strings.
    constexpr int kStateNone        = 0;
    constexpr int kStateTripleDouble = 1;
    constexpr int kStateTripleSingle = 2;
}

PythonHighlighter::PythonHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {

    // Keywords — Python 3 reserved words plus the common pseudo-keywords
    // that visually behave the same (True / False / None / self).
    QTextCharFormat keywordFmt;
    keywordFmt.setForeground(QColor(220, 160, 80));
    keywordFmt.setFontWeight(QFont::Bold);
    const QStringList keywords = {
        QStringLiteral("\\bdef\\b"),    QStringLiteral("\\bclass\\b"),
        QStringLiteral("\\bif\\b"),     QStringLiteral("\\belif\\b"),
        QStringLiteral("\\belse\\b"),   QStringLiteral("\\bfor\\b"),
        QStringLiteral("\\bwhile\\b"),  QStringLiteral("\\breturn\\b"),
        QStringLiteral("\\bimport\\b"), QStringLiteral("\\bfrom\\b"),
        QStringLiteral("\\bas\\b"),     QStringLiteral("\\btry\\b"),
        QStringLiteral("\\bexcept\\b"), QStringLiteral("\\bfinally\\b"),
        QStringLiteral("\\bwith\\b"),   QStringLiteral("\\byield\\b"),
        QStringLiteral("\\blambda\\b"), QStringLiteral("\\bpass\\b"),
        QStringLiteral("\\bbreak\\b"),  QStringLiteral("\\bcontinue\\b"),
        QStringLiteral("\\bTrue\\b"),   QStringLiteral("\\bFalse\\b"),
        QStringLiteral("\\bNone\\b"),   QStringLiteral("\\band\\b"),
        QStringLiteral("\\bor\\b"),     QStringLiteral("\\bnot\\b"),
        QStringLiteral("\\bis\\b"),     QStringLiteral("\\bin\\b"),
        QStringLiteral("\\bglobal\\b"), QStringLiteral("\\bnonlocal\\b"),
        QStringLiteral("\\braise\\b"),  QStringLiteral("\\bassert\\b"),
        QStringLiteral("\\bself\\b"),
    };
    for (const QString& k : keywords) {
        m_rules.push_back({QRegularExpression(k), keywordFmt});
    }

    // Builtins / common functions — a small curated set. Not exhaustive;
    // missing names just go un-coloured rather than wrong-coloured.
    QTextCharFormat builtinFmt;
    builtinFmt.setForeground(QColor(100, 200, 200));
    for (const QString& b : {
             QStringLiteral("\\bprint\\b"),  QStringLiteral("\\blen\\b"),
             QStringLiteral("\\brange\\b"),  QStringLiteral("\\bint\\b"),
             QStringLiteral("\\bfloat\\b"),  QStringLiteral("\\bstr\\b"),
             QStringLiteral("\\blist\\b"),   QStringLiteral("\\bdict\\b"),
             QStringLiteral("\\btuple\\b"),  QStringLiteral("\\bset\\b"),
             QStringLiteral("\\bmin\\b"),    QStringLiteral("\\bmax\\b"),
             QStringLiteral("\\babs\\b"),    QStringLiteral("\\bround\\b"),
             QStringLiteral("\\benumerate\\b"), QStringLiteral("\\bzip\\b"),
             QStringLiteral("\\bisinstance\\b"),
         }) {
        m_rules.push_back({QRegularExpression(b), builtinFmt});
    }

    // Numbers — integers and decimals, no exponents (cheap heuristic).
    QTextCharFormat numberFmt;
    numberFmt.setForeground(QColor(200, 130, 200));
    m_rules.push_back({QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")),
                       numberFmt});

    // Decorators — `@name` on its own line, typically.
    QTextCharFormat decoratorFmt;
    decoratorFmt.setForeground(QColor(220, 220, 120));
    m_rules.push_back({QRegularExpression(QStringLiteral("@[A-Za-z_][A-Za-z_0-9]*")),
                       decoratorFmt});

    // The name following `def` or `class`.
    QTextCharFormat defFmt;
    defFmt.setForeground(QColor(240, 240, 240));
    defFmt.setFontWeight(QFont::Bold);
    m_rules.push_back({QRegularExpression(QStringLiteral("(?<=\\bdef\\s)\\w+")), defFmt});
    m_rules.push_back({QRegularExpression(QStringLiteral("(?<=\\bclass\\s)\\w+")), defFmt});

    // Single-line strings — both quote flavours, with escape support.
    QTextCharFormat stringFmt;
    stringFmt.setForeground(QColor(150, 200, 130));
    m_rules.push_back({QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"")),
                       stringFmt});
    m_rules.push_back({QRegularExpression(QStringLiteral("'(?:\\\\.|[^'\\\\])*'")),
                       stringFmt});

    // Single-line comments — last so they win over earlier rules on the line.
    QTextCharFormat commentFmt;
    commentFmt.setForeground(QColor(120, 120, 130));
    commentFmt.setFontItalic(true);
    m_rules.push_back({QRegularExpression(QStringLiteral("#[^\\n]*")), commentFmt});

    // Triple-quoted strings — kept separate, applied in highlightBlock with
    // block-state tracking so they cross line boundaries.
    m_stringFmt = stringFmt;
}

void PythonHighlighter::highlightBlock(const QString& text) {
    // ── Apply single-line rules first ────────────────────────────────────────
    for (const auto& rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(static_cast<int>(m.capturedStart()),
                      static_cast<int>(m.capturedLength()),
                      rule.format);
        }
    }

    // ── Triple-quoted strings (multi-line) ───────────────────────────────────
    //
    // Walk the line scanning for triple-quote delimiters. Block state carries
    // "we are inside a triple-quoted string of flavour X" across line breaks.
    setCurrentBlockState(kStateNone);

    int startIndex = 0;
    int state      = previousBlockState();
    if (state != kStateTripleDouble && state != kStateTripleSingle) {
        state = kStateNone;
    }

    while (startIndex < text.length()) {
        if (state == kStateNone) {
            // Look for the next opening triple-quote.
            const auto md = m_tripleDouble.match(text, startIndex);
            const auto ms = m_tripleSingle.match(text, startIndex);
            int dPos = md.hasMatch() ? static_cast<int>(md.capturedStart()) : -1;
            int sPos = ms.hasMatch() ? static_cast<int>(ms.capturedStart()) : -1;
            if (dPos < 0 && sPos < 0) break;

            int open;
            if (dPos < 0)         { open = sPos; state = kStateTripleSingle; }
            else if (sPos < 0)    { open = dPos; state = kStateTripleDouble; }
            else if (dPos < sPos) { open = dPos; state = kStateTripleDouble; }
            else                  { open = sPos; state = kStateTripleSingle; }
            startIndex = open;
        }
        // Inside a triple-quoted string starting at `startIndex` of flavour
        // determined by `state`. Find the closing delimiter on this line.
        const QRegularExpression& closing =
            (state == kStateTripleDouble) ? m_tripleDouble : m_tripleSingle;
        const auto endMatch = closing.match(text, startIndex + 3);
        if (endMatch.hasMatch()) {
            const int endPos = static_cast<int>(endMatch.capturedEnd());
            setFormat(startIndex, endPos - startIndex, m_stringFmt);
            startIndex = endPos;
            state = kStateNone;
        } else {
            // Runs off the end of the line; carry state into the next block.
            setFormat(startIndex, text.length() - startIndex, m_stringFmt);
            setCurrentBlockState(state);
            return;
        }
    }
}
