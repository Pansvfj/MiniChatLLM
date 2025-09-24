#include "stdafx.h"

bool g_onlineInfer = false;

int getTextWidth(const QFont& font, const QString& str)
{
	QFontMetrics metrics(font);
	return metrics.horizontalAdvance(str);
}