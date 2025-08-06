#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "LLMRunner.h"

class ChatWindow : public QWidget
{
	Q_OBJECT
public:
	explicit ChatWindow(QWidget* parent = nullptr);

private slots:
	void onSendClicked();
	void onChatResult(const QString& reply);

private:
	QTextEdit* m_chatView;
	QLineEdit* m_input;
	QPushButton* m_sendBtn;
	LLMRunner* m_llm;
};

#endif // CHATWINDOW_H
