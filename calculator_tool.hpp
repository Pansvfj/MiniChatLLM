#pragma once

#include <QString>
#include <QRegExp>

class CalculatorTool {
public:
    static QString calculate(const QString& expr) {
        QRegExp rx(R"(([-+]?\d+(\.\d+)?)[\s]*([\+\-\*/])[\s]*([-+]?\d+(\.\d+)?))");
        if (rx.indexIn(expr) != -1) {
            double a = rx.cap(1).toDouble();
            QString op = rx.cap(3);
            double b = rx.cap(4).toDouble();
            if (op == "+") return QString::number(a + b);
            if (op == "-") return QString::number(a - b);
            if (op == "*") return QString::number(a * b);
            if (op == "/") return b != 0 ? QString::number(a / b) : QString("除数为0");
        }
        return QStringLiteral("");
    }
};
