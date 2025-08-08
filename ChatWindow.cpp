#include "stdafx.h"
#include "ChatWindow.h"
#include "LoadingTipWidget.h"


ChatWindow::ChatWindow(QWidget* parent)
	: QWidget(parent)
{
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

	m_llm = new LLMRunner("D:\\Projects\\MiniChatLLM\\models\\qwen1_5-0_5b-chat-q4_0.gguf", this);
	m_futureInit = QtConcurrent::run([this]() {
		m_llm->init();
	});
	if (!m_llm) {
		QMessageBox::warning(nullptr, "Error", "LLM is NULL");
	}
	connect(m_llm, &LLMRunner::signalInitLLMFinished, m_loadingTip, &LoadingTipWidget::stopAndClose);
	connect(m_llm, &LLMRunner::chatResult, this, &ChatWindow::onChatResult);

	connect(m_sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
	connect(m_input, &QLineEdit::returnPressed, this, &ChatWindow::onSendClicked);
	m_processingTimer.setInterval(1000); // 每秒更新一次
	connect(&m_processingTimer, &QTimer::timeout, this, [=]() {
		m_secondsThinking++;
	});

	m_input->setFocus();
	m_loadingTip->exec();
}

ChatWindow::~ChatWindow()
{
	// 断开 m_llm 所有与 ChatWindow 的信号槽连接
	if (m_llm) {
		disconnect(m_llm, nullptr, this, nullptr);
		m_llm->requestAbort();
	}

	// 等待任务结束
	if (m_futureInit.isRunning()) m_futureInit.waitForFinished();
	if (m_futureChat.isRunning()) m_futureChat.waitForFinished();

	// m_llm 会由 Qt 自动释放，因为它的 parent 是 this
	// 手动 delete m_llm; 会有重复释放风险

	delete m_loadingTip; // m_loadingTip 不是 QObject 子对象才需要手动
}


void ChatWindow::closeEvent(QCloseEvent* e)
{
	QWidget::closeEvent(e);
	m_loadingTip->close();
}

void ChatWindow::onSendClicked()
{
	QString inputText = m_input->text().trimmed();
	if (inputText.isEmpty()) return;

	// 记录历史并清空输入框
	m_chatView->append("<b>我:</b> " + inputText);
	m_input->clear();
	m_lastUserInput = inputText;

	// 启动后台线程执行推理
	m_futureChat = QtConcurrent::run([this, inputText]() {
		QString reply = m_llm->chat(inputText);
	});

	m_chatView->append(tr("AI 思考中..."));
	m_aiProcessing = true;
	m_processingTimer.start();
}


void ChatWindow::onChatResult(const QString& reply)
{
	if (m_aiProcessing) {
		m_aiProcessing = false;
		m_chatView->append(tr("已思考%1s").arg(QString::number(m_secondsThinking)));
		m_secondsThinking = 0;
		m_processingTimer.stop();
	}
	m_chatView->append("<b>LLM:</b> " + reply);
}
