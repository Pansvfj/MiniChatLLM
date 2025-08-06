#include "stdafx.h"
#include "ChatWindow.h"
#include <QHBoxLayout>

ChatWindow::ChatWindow(QWidget* parent)
	: QWidget(parent)
{
	m_chatView = new QTextEdit(this);
	m_chatView->setReadOnly(true);
	m_input = new QLineEdit(this);
	m_sendBtn = new QPushButton("����", this);

	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->addWidget(m_chatView);

	QHBoxLayout* inputLayout = new QHBoxLayout();
	inputLayout->addWidget(m_input);
	inputLayout->addWidget(m_sendBtn);

	mainLayout->addLayout(inputLayout);
	setLayout(mainLayout);

	m_llm = new LLMRunner("D:\\Projects\\MiniChatLLM\\models\\Yi-1.5-6B-Chat-Q4_K_M.gguf", this);
	if (!m_llm) {
		QMessageBox::warning(nullptr, "Error", "LLM is NULL");
	}

	connect(m_sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
	connect(m_llm, &LLMRunner::chatResult, this, &ChatWindow::onChatResult);
	connect(m_input, &QLineEdit::returnPressed, this, &ChatWindow::onSendClicked);

	m_input->setFocus();
}

void ChatWindow::onSendClicked()
{
	QString inputText = m_input->text().trimmed();
	if (inputText.isEmpty()) return;
	m_chatView->append("<b>��:</b> " + inputText);
	m_input->clear();
	// ͬ���棨���費��UI�����̻߳���QtConcurrent::run��
	QString reply = m_llm->chat(inputText);
	// onChatResult���Զ�append��emit�ź������첽��
}

void ChatWindow::onChatResult(const QString& reply)
{
	m_chatView->append("<b>LLM:</b> " + reply);
}
