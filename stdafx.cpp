#include "stdafx.h"

int getTextWidth(const QFont& font, const QString& str)
{
	QFontMetrics metrics(font);
	return metrics.horizontalAdvance(str);
}