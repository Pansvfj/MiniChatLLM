#include "stdafx.h"
#include "ChatWindow.h"
#include "LoadingTipWidget.h"

ChatWindow::ChatWindow(QWidget* parent)
	: QWidget(parent) {
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

	m_loadingTip = new LoadingTipWidget;


#if USE_SIMPLE
	m_llm = new LLMRunner("D:\\Projects\\MiniChatLLM\\models\\qwen1_5-0_5b-chat-q4_0.gguf", this);
#else
	m_llm = new LLMRunnerYi("D:\\Projects\\MiniChatLLM\\models\\Yi-1.5-6B-Chat-Q4_K_M.gguf", this);
#endif
	m_futureInit = QtConcurrent::run([this]() {
		m_llm->init();
		});
	if (!m_llm) {
		QMessageBox::warning(nullptr, "Error", "LLM is NULL");
	}
#if USE_SIMPLE
	connect(m_llm, &LLMRunner::signalInitLLMFinished, m_loadingTip, &LoadingTipWidget::stopAndClose);
	connect(m_llm, &LLMRunner::chatResult, this, &ChatWindow::onChatResult);
	// �� ��ʽ��Ƭ
	connect(m_llm, &LLMRunner::chatStreamResult, this, &ChatWindow::onChatStreamResult);
#else
	connect(m_llm, &LLMRunnerYi::signalInitLLMFinished, m_loadingTip, &LoadingTipWidget::stopAndClose);
	connect(m_llm, &LLMRunnerYi::chatResult, this, &ChatWindow::onChatResult);
#endif



	connect(m_sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
	connect(m_input, &QLineEdit::returnPressed, this, &ChatWindow::onSendClicked);

	m_processingTimer.setInterval(1000);
	connect(&m_processingTimer, &QTimer::timeout, this, [=]() { m_secondsThinking++; });

	m_input->setFocus();
	m_loadingTip->exec();
}

ChatWindow::~ChatWindow() {
	// �Ͽ��ź� + ������ֹ��ǰ����
	if (m_llm) {
		disconnect(m_llm, nullptr, this, nullptr);
		m_llm->requestAbort();
	}
	if (m_futureInit.isRunning()) m_futureInit.waitForFinished();
	if (m_futureChat.isRunning()) m_futureChat.waitForFinished();

	delete m_loadingTip;
}

void ChatWindow::closeEvent(QCloseEvent* e) {
	QWidget::closeEvent(e);
	if (m_llm) m_llm->requestAbort();
	m_loadingTip->close();
}

void ChatWindow::onSendClicked() {
	const QString inputText = m_input->text().trimmed();
	if (inputText.isEmpty()) return;

	// ����һ�ֻ������ɣ��ȳ�����ֹ
	if (m_llm) m_llm->requestAbort();
	if (m_futureChat.isRunning()) m_futureChat.waitForFinished();

	// ��¼��ʷ����������
	m_chatView->append("<b>��:</b> " + inputText);
	m_input->clear();
	m_lastUserInput = inputText;

	// ״̬��ʾ
	m_chatView->append(tr("AI ˼����..."));
	m_aiThinking = true;
	m_secondsThinking = 0;
	m_processingTimer.start();

	// Ԥ��һ�С�LLM: ������������ʽ token ���ӵ����к���
	m_chatView->append("<b>LLM:</b> ");
	// �ѹ���ƶ���ĩβ��׼����ʽ����
	QTextCursor c = m_chatView->textCursor();
	c.movePosition(QTextCursor::End);
	m_chatView->setTextCursor(c);

	// ��̨����
	m_futureChat = QtConcurrent::run([this, inputText]() {
		(void)m_llm->chat(inputText);
		});
}

void ChatWindow::appendInline(const QString& text) {
	QTextCursor cursor = m_chatView->textCursor();
	cursor.movePosition(QTextCursor::End);
	cursor.insertText(text);
	m_chatView->setTextCursor(cursor);
	m_chatView->ensureCursorVisible();
}

void ChatWindow::onChatStreamResult(const QString& partial) {
	if (m_aiThinking) {
		m_aiThinking = false;
		m_processingTimer.stop();
		// �ڡ�LLM:����һ���Ѿ���������ݵ���ʽ׷�ӣ�����ֻ��״̬��β
		m_chatView->append(tr("��˼��%1s").arg(QString::number(m_secondsThinking)));
		m_secondsThinking = 0;
	}

	// �ѷ�Ƭֱ�Ӳ嵽��ǰ��ĩβ��������
	appendInline(partial);
}

void ChatWindow::onChatResult(const QString& reply) {
	// �����������ս�����������һ�飬���Խ⿪�������У�Ĭ�ϲ��ظ������
	// m_chatView->append("<b>LLM(����):</b> " + reply);
}
