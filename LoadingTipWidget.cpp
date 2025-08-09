#include "stdafx.h"
#include "LoadingTipWidget.h"
#include <QVBoxLayout>
#include <QFont>

LoadingTipWidget::LoadingTipWidget(QWidget* parent)
	: QWidget(parent), m_elapsedSeconds(0)
{
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
	setAttribute(Qt::WA_TranslucentBackground);
	setFixedSize(300, 100);

	m_label = new QLabel(this);
	m_label->setAlignment(Qt::AlignCenter);
	m_label->setText(tr("加载模型中……0秒"));
	m_label->setStyleSheet("color: white; background-color: rgba(0, 0, 0, 180); padding: 10px; border-radius: 10px;");
	m_label->setFont(QFont("Microsoft YaHei", 14));

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->addWidget(m_label);
	setLayout(layout);

	m_timer = new QTimer(this);
	connect(m_timer, &QTimer::timeout, this, &LoadingTipWidget::updateTimer);
}

int LoadingTipWidget::exec()
{
	m_elapsedSeconds = 0;
	m_label->setText(tr("加载模型中……0秒"));
	m_timer->start(1000);
	show();           // 弹出窗口
	m_eventLoop.exec(); // 阻塞，直到 stopAndClose() 被调用
	return 0;
}

void LoadingTipWidget::updateTimer()
{
	m_elapsedSeconds++;
	m_label->setText(tr("加载模型中……%1秒").arg(m_elapsedSeconds));
}

void LoadingTipWidget::stopAndClose()
{
	if (m_timer->isActive())
		m_timer->stop();
	close();
	m_eventLoop.quit();  // 退出 exec 阻塞
}
