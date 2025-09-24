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
	// ��������
	QString cleanAndUnescape(const QString& raw);

	QNetworkAccessManager* manager;
	QString serverUrl;

	QString botReplyBuffer;   // ���������ظ�����λ� finished ��õ���
	int bufferPos;            // ����ʾ���ַ�λ��
	bool replyFinished;       // ������Ƿ�����ɣ�finished �źţ�
	QTimer* displayTimer;     // ����������ʾ

	// ������ÿ�� reply ���ۻ�����
	QHash<QNetworkReply*, QByteArray> replyBufferMap;
};

#endif // ONLINEINFETWINDOW_H
