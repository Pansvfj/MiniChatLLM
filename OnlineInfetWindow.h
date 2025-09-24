#ifndef ONLINEINFETWINDOW_H
#define ONLINEINFETWINDOW_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QHash>

class OnlineInfetWindow : public QWidget
{
	Q_OBJECT

public:
	explicit OnlineInfetWindow(const QString& url, QWidget* parent = nullptr);

private slots:
	void onSendButtonClicked();
	void sendRequest(const QString& text);
	void onReplyReadyRead();
	void onReplyFinished();
	void updateBotReply();

private:
	// 帮助函数
	QString cleanAndUnescape(const QString& raw);

	QNetworkAccessManager* manager;
	QString serverUrl;

	QString botReplyBuffer;   // 缓存完整回复（逐段或 finished 后得到）
	int bufferPos;            // 已显示的字符位置
	bool replyFinished;       // 网络层是否已完成（finished 信号）
	QTimer* displayTimer;     // 用于增量显示

	// 新增：每个 reply 的累积缓冲
	QHash<QNetworkReply*, QByteArray> replyBufferMap;
};

#endif // ONLINEINFETWINDOW_H
