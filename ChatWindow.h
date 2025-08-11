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
#include <QComboBox>
#include <QLabel>

#include "LLMRunner.h"

class LoadingTipWidget;
class LLMRunnerYi;

class ChatWindow : public QWidget {
	Q_OBJECT
public:
	explicit ChatWindow(const QString& modelFilePath, QWidget* parent = nullptr);
	~ChatWindow();

protected:
	void closeEvent(QCloseEvent* e) override;

private slots:
	void onSendClicked();
	void onChatStreamResult(const QString& partial);
	void onChatResult(const QString& reply);

private:
	void appendInline(const QString& text);

private:
	// 顶部条：模式选择
	QComboBox* m_modeBox = nullptr;
	QLabel* m_modeLbl = nullptr;

	QTextEdit* m_chatView;
	QLineEdit* m_input;
	QPushButton* m_sendBtn;
	LLMRunner* m_llm;
	QString m_lastUserInput;

	bool m_aiThinking = false;
	QTimer m_processingTimer;
	int m_secondsThinking = 0;
	LoadingTipWidget* m_loadingTip = nullptr;

	QFuture<void> m_futureInit;
	QFuture<void> m_futureChat;
};

#endif // CHATWINDOW_H
