#pragma once
// Built-in translations (Polish) without Qt Linguist tools: a QTranslator that looks the English
// source text up in a compiled-in table. Language: Settings > General (automatic / English / Polski).
#include <QTranslator>

class AppTranslator : public QTranslator
{
public:
    explicit AppTranslator(bool polish, QObject *parent = nullptr) : QTranslator(parent), m_polish(polish) {}
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
                      int n = -1) const override;
    bool isEmpty() const override { return false; }
private:
    bool m_polish;
};

// "auto", "en" or "pl" -> true when the Polish texts should be used
bool usePolish(const QString &setting);
