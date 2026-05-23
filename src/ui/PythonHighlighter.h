#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <vector>

// ── PythonHighlighter ────────────────────────────────────────────────────────
//
// QSyntaxHighlighter for Python source. Covers keywords, builtins, numbers,
// decorators, def/class names, single-line comments, single-line strings,
// and triple-quoted string blocks (tracked via QTextBlock::previousBlockState
// so a """…""" spanning many lines highlights correctly).

class PythonHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit PythonHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };

    // Single-line rules applied in order over the block. Tracks for triple-
    // quoted strings live separately because they span blocks.
    std::vector<Rule>   m_rules;
    QTextCharFormat     m_stringFmt;
    QRegularExpression  m_tripleDouble{ QStringLiteral("\"\"\"") };
    QRegularExpression  m_tripleSingle{ QStringLiteral("'''")    };
};
