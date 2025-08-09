#include "stdafx.h"
#include "ChatWindow.h"
#include "LoadingTipWidget.h"

ChatWindow::ChatWindow(QWidget* parent)
	: QWidget(parent) {
	m_chatView = new QTextEdit(this);
	m_chatView->setReadOnly(true);
	m_input = new QLineEdit(this);
	m_sendBtn = new QPushButton("发送", this);

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
	// ★ 流式分片
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
	// 断开信号 + 请求终止当前生成
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

	// 若上一轮还在生成，先尝试中止
	if (m_llm) m_llm->requestAbort();
	if (m_futureChat.isRunning()) m_futureChat.waitForFinished();

	// 记录历史并清空输入框
	m_chatView->append("<b>我:</b> " + inputText);
	m_input->clear();
	m_lastUserInput = inputText;

	// 状态提示
	m_chatView->append(tr("AI 思考中..."));
	m_aiThinking = true;
	m_secondsThinking = 0;
	m_processingTimer.start();

	// 预置一行“LLM: ”，后续的流式 token 都接到这行后面
	m_chatView->append("<b>LLM:</b> ");
	// 把光标移动到末尾，准备流式插入
	QTextCursor c = m_chatView->textCursor();
	c.movePosition(QTextCursor::End);
	m_chatView->setTextCursor(c);

	// 后台生成
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
		// 在“LLM:”这一行已经完成了内容的流式追加，这里只做状态收尾
		m_chatView->append(tr("已思考%1s").arg(QString::number(m_secondsThinking)));
		m_secondsThinking = 0;
	}

	// 把分片直接插到当前行末尾，不换行
	appendInline(partial);
}

void ChatWindow::onChatResult(const QString& reply) {
	// 如果还想把最终结果再整体回显一遍，可以解开下面这行（默认不重复输出）
	// m_chatView->append("<b>LLM(完整):</b> " + reply);
}
