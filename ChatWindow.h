#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

#include "LLMRunner.h"

class LoadingTipWidget;

class ChatWindow : public QWidget {
	Q_OBJECT
public:
	explicit ChatWindow(QWidget* parent = nullptr);
	~ChatWindow();

protected:
	void closeEvent(QCloseEvent* e) override;

private slots:
	void onSendClicked();
	void onChatResult(const QString& reply);

private:
	QTextEdit* m_chatView;
	QLineEdit* m_input;
	QPushButton* m_sendBtn;
	LLMRunner* m_llm;
	QString m_lastUserInput;

	bool m_aiProcessing = false;
	QTimer m_processingTimer;
	int m_secondsThinking = 0;
	LoadingTipWidget* m_loadingTip = nullptr;

	//新增：跟踪任务
	QFuture<void> m_futureInit;
	QFuture<void> m_futureChat;
};


#endif // CHATWINDOW_H
