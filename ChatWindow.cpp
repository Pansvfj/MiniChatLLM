#include "stdafx.h"
#include "ChatWindow.h"
#include "LoadingTipWidget.h"

ChatWindow::ChatWindow(const QString& modelFilePath, QWidget* parent)
	: QWidget(parent) {
	// ����ģʽ��
	m_modeLbl = new QLabel("ģʽ��", this);
	m_modeBox = new QComboBox(this);
	m_modeBox->addItem("���ף�̰�� + �̴�", QVariant(0));      // LLMRunner::Preset::SafeGreedy
	m_modeBox->addItem("ƽ�⣺top-p 0.7 / temp 0.2", QVariant(1)); // LLMRunner::Preset::Balanced
	m_modeBox->setCurrentIndex(1); // Ĭ�ϡ�ƽ�⡱

	// ��������������
	m_chatView = new QTextEdit(this);
	m_chatView->setReadOnly(true);
	m_input = new QLineEdit(this);
	m_sendBtn = new QPushButton("����", this);

	QVBoxLayout* mainLayout = new QVBoxLayout(this);

	// ��ͷλ�ò���
	QHBoxLayout* topBar = new QHBoxLayout();
	topBar->addWidget(m_modeLbl);
	topBar->addWidget(m_modeBox);
	topBar->addStretch(1);

	mainLayout->addLayout(topBar);      // �� ��ͷ����������
	mainLayout->addWidget(m_chatView);  // ������ʾ

	QHBoxLayout* inputLayout = new QHBoxLayout();
	inputLayout->addWidget(m_input);
	inputLayout->addWidget(m_sendBtn);
	mainLayout->addLayout(inputLayout);

	setLayout(mainLayout);

	

	m_loadingTip = new LoadingTipWidget;
	m_llm = new LLMRunner(modelFilePath, this);

	if (!m_llm) {
		QMessageBox::warning(nullptr, "Error", "LLM is NULL");
	}
	connect(m_llm, &LLMRunner::signalInitLLMFinished, m_loadingTip, &LoadingTipWidget::stopAndClose);
	m_futureInit = QtConcurrent::run([this]() { m_llm->init(); });

	connect(m_llm, &LLMRunner::chatResult, this, &ChatWindow::onChatResult);
	connect(m_llm, &LLMRunner::chatStreamResult, this, &ChatWindow::onChatStreamResult);

	// �������򵽲���Ԥ��
	connect(m_modeBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [this](int idx) {
			m_llm->setPreset(static_cast<LLMRunner::Preset>(idx));
		});
	// ��ʼ��һ��
	m_llm->setPreset(static_cast<LLMRunner::Preset>(m_modeBox->currentIndex()));

	connect(m_sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
	connect(m_input, &QLineEdit::returnPressed, this, &ChatWindow::onSendClicked);

	m_processingTimer.setInterval(1000);
	connect(&m_processingTimer, &QTimer::timeout, this, [=]() { m_secondsThinking++; });

	m_input->setFocus();
	m_loadingTip->exec();
}

ChatWindow::~ChatWindow() {
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

	if (m_llm) m_llm->requestAbort();
	if (m_futureChat.isRunning()) m_futureChat.waitForFinished();

	m_chatView->append("<b>��:</b> " + inputText);
	m_input->clear();
	m_lastUserInput = inputText;

	m_chatView->append(tr("AI ˼����..."));
	m_aiThinking = true;
	m_secondsThinking = 0;
	m_processingTimer.start();

	m_chatView->append("<b>LLM:</b> ");
	QTextCursor c = m_chatView->textCursor();
	c.movePosition(QTextCursor::End);
	m_chatView->setTextCursor(c);

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
		// �������У��ĳ�״̬д�ڱ�����ĩβ��������
		appendInline(QString("����˼��%1s�� ").arg(m_secondsThinking));
		m_secondsThinking = 0;
	}
	appendInline(partial); // ������ͬһ��׷д
}

void ChatWindow::onChatResult(const QString& reply) {
	// �������������ԣ����ڴ�׷��
}
