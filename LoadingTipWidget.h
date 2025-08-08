#ifndef LOADINGTIPWIDGET_H
#define LOADINGTIPWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QEventLoop>

class LoadingTipWidget : public QWidget
{
	Q_OBJECT
public:
	explicit LoadingTipWidget(QWidget* parent = nullptr);

	// 类似 QDialog::exec()，阻塞式弹出
	int exec(); // 返回 0 表示被 stopAndClose() 正常关闭

public slots:
	void stopAndClose(); // 外部调用以关闭并退出 exec()

private slots:
	void updateTimer();

private:
	QLabel* m_label;
	QTimer* m_timer;
	int m_elapsedSeconds;
	QEventLoop m_eventLoop;
};

#endif // LOADINGTIPWIDGET_H
