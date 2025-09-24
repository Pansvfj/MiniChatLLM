#ifndef STDAFX_H
#define STDAFX_H

#pragma execution_character_set("utf-8")

#include <vector>
#include <utility>
#include <string>

#include <QDebug>

#include <QObject>
#include <QMetaType>
#include <QFile>
#include <QTextBrowser>
#include <QTimer>
#include <QStyle>
#include <QFontMetrics>
#include <QFont>
#include <QVBoxLayout>
#include <QImage>
#include <QStringList>
#include <QThread>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTextCursor>
#include <QtConcurrent>
#include <QTextBlock>

#include "PrivacyData.hpp"

struct YoloDetection {
	QRect bbox;
	QString label;
	float confidence;
};

int getTextWidth(const QFont& font, const QString& str);

extern bool g_onlineInfer;

#endif	// STDAFX_H