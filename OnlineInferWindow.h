#ifndef ONLINEINFERWINDOW_H
#define ONLINEINFERWINDOW_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QMap>
#include <QNetworkReply>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>

class OnlineInferWindow : public QWidget
{
	Q_OBJECT

public:
	explicit OnlineInferWindow(const QString& url, QWidget* parent = nullptr);
	~OnlineInferWindow() override;

	QString retrieveKnowledge(const QString& userInput);
	bool isCalculationRequest(const QString& input);

private slots:
	void onSendButtonClicked();
	void sendRequestWithHistory();
	void onReplyReadyRead();
	void onReplyFinished();
	void updateBotReply();

private:
	QString cleanAndUnescape(const QString& raw);
	QString cleanAIOutput(const QString& raw);

	// 网络相关成员
	QNetworkAccessManager* manager;
	QString serverUrl;
	QMap<QNetworkReply*, QByteArray> replyBufferMap;

	// UI组件
	QTextBrowser* textBrowser;
	QLineEdit* lineEdit;
	QPushButton* sendButton;

	// 回复处理相关
	QTimer* displayTimer;
	QString botReplyBuffer;
	int bufferPos;
	bool replyFinished;

	struct ChatMessage {
		QString role;
		QString content;
	};
	QVector<ChatMessage> chatHistory;  // 全局或类成员
	const int MAX_HISTORY = 10;         // 保留最近 10 条消息
	QPushButton* m_newSessionButton = nullptr;  // 新会话按钮

};

#endif // ONLINEINFERWINDOW_H
