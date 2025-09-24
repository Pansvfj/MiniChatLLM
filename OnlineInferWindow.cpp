#include "stdafx.h"
#include "OnlineInferWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTextCursor>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QPushButton>
#include <QLineEdit>
#include <QTextBrowser>
#include <QScrollBar>

OnlineInferWindow::OnlineInferWindow(const QString& url, QWidget* parent)
	: QWidget(parent),
	manager(new QNetworkAccessManager(this)),
	serverUrl(url),
	bufferPos(0),
	replyFinished(false)
{
	// 初始化UI组件
	textBrowser = new QTextBrowser(this);
	textBrowser->setObjectName("textBrowser");
	textBrowser->setReadOnly(true);
	textBrowser->setOpenExternalLinks(true);

	lineEdit = new QLineEdit(this);
	lineEdit->setObjectName("lineEdit");
	lineEdit->setPlaceholderText(tr("输入消息..."));

	sendButton = new QPushButton(tr("发送"), this);
	sendButton->setObjectName("sendButton");

	// 布局管理
	auto mainLayout = new QVBoxLayout(this);
	mainLayout->addWidget(textBrowser);

	// 定时器初始化
	displayTimer = new QTimer(this);
	displayTimer->setInterval(40);  // 控制显示速度

	// 信号槽连接
	connect(sendButton, &QPushButton::clicked, this, &OnlineInferWindow::onSendButtonClicked);
	connect(lineEdit, &QLineEdit::returnPressed, sendButton, &QPushButton::click);
	connect(displayTimer, &QTimer::timeout, this, &OnlineInferWindow::updateBotReply);

	displayTimer->start();
	lineEdit->setFocus();

	m_newSessionButton = new QPushButton(tr("新会话"), this);

	auto inputLayout = new QHBoxLayout();
	inputLayout->addWidget(lineEdit);
	inputLayout->addWidget(sendButton);
	inputLayout->addWidget(m_newSessionButton);
	mainLayout->addLayout(inputLayout);

	connect(m_newSessionButton, &QPushButton::clicked, this, [=]() {
		// 1. 清空聊天显示
		textBrowser->clear();

		// 2. 清空上下文
		chatHistory.clear();

		// 3. 清空AI回复缓冲
		botReplyBuffer.clear();
		bufferPos = 0;
		replyFinished = false;

		// 4. 停止所有未完成的网络请求
		for (auto reply : replyBufferMap.keys()) {
			if (reply) {
				reply->abort();
				reply->deleteLater();
			}
		}
		replyBufferMap.clear();

		// 5. 光标回到输入框
		lineEdit->setFocus();
	});
}

OnlineInferWindow::~OnlineInferWindow()
{
	// 清理所有未完成的网络请求
	for (auto reply : replyBufferMap.keys()) {
		if (reply) {
			reply->abort();
			reply->deleteLater();
		}
	}
}

void OnlineInferWindow::onSendButtonClicked()
{
	sendButton->setEnabled(false);
	
	const QString text = lineEdit->text().trimmed();
	if (text.isEmpty()) return;

	// 显示用户输入
	textBrowser->append(QString("<b>你:</b> %1").arg(text));
	lineEdit->clear();

	// 保存用户消息到历史
	chatHistory.append({ "user", text });
	if (chatHistory.size() > MAX_HISTORY)
		chatHistory.removeFirst();

	// 重置回复缓冲区
	botReplyBuffer.clear();
	bufferPos = 0;
	replyFinished = false;

	// 准备显示AI回复
	textBrowser->append("<b>AI:</b> ");

	textBrowser->verticalScrollBar()->setValue(textBrowser->verticalScrollBar()->maximum());

	// 发送请求时带上下文
	sendRequestWithHistory();
}

void OnlineInferWindow::sendRequestWithHistory()
{
	QJsonObject jsonRequest;
	QJsonArray messagesArray;

	for (const ChatMessage& msg : chatHistory) {
		QJsonObject obj;
		obj["role"] = msg.role;
		obj["content"] = msg.content;
		messagesArray.append(obj);
	}

	jsonRequest["messages"] = messagesArray;
	jsonRequest["max_tokens"] = 4096;

	QJsonDocument jsonDoc(jsonRequest);
	QByteArray requestData = jsonDoc.toJson(QJsonDocument::Compact);

	QUrl url(serverUrl);
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply* reply = manager->post(request, requestData);
	replyBufferMap.insert(reply, QByteArray());

	connect(reply, &QNetworkReply::readyRead, this, &OnlineInferWindow::onReplyReadyRead);
	connect(reply, &QNetworkReply::finished, this, &OnlineInferWindow::onReplyFinished);

	qDebug() << "已发送请求 (长度=" << requestData.size() << ")";
}


void OnlineInferWindow::onReplyReadyRead()
{
	auto reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) return;

	// 读取并缓存数据
	const QByteArray chunk = reply->readAll();
	if (!chunk.isEmpty()) {
		replyBufferMap[reply].append(chunk);
		qDebug() << "接收数据片段 (长度=" << chunk.size() << ") 预览:" << chunk.left(256);
	}
}

void OnlineInferWindow::onReplyFinished()
{
	auto reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) return;

	// 获取完整响应数据
	QByteArray fullData = replyBufferMap.take(reply);
	if (fullData.isEmpty()) {
		fullData = reply->readAll();
	}

	// 网络错误处理
	if (reply->error() != QNetworkReply::NoError) {
		textBrowser->append(QString("<span style='color:red;'><b>错误:</b> %1</span>")
			.arg(reply->errorString()));
		botReplyBuffer = QString("\n[错误详情]\n") + QString::fromUtf8(fullData);
	}
	else {
		// 正常响应处理
		QJsonParseError parseError;
		QJsonDocument jsonDoc = QJsonDocument::fromJson(fullData, &parseError);

		if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
			QJsonObject jsonResponse = jsonDoc.object();
			QString rawContent;

			if (jsonResponse.contains("choices") && jsonResponse["choices"].isArray()) {
				QJsonArray choices = jsonResponse["choices"].toArray();
				if (!choices.isEmpty()) {
					QJsonValue firstChoice = choices.first();
					if (firstChoice.isObject()) {
						QJsonObject choiceObj = firstChoice.toObject();
						if (choiceObj.contains("text")) {
							rawContent = choiceObj["text"].toString();
						}
						else if (choiceObj.contains("message") && choiceObj["message"].isObject()) {
							rawContent = choiceObj["message"].toObject()["content"].toString();
						}
					}
					else if (firstChoice.isString()) {
						rawContent = firstChoice.toString();
					}

					if (rawContent.isEmpty()) {
						rawContent = QString::fromUtf8(fullData);
					}

					botReplyBuffer = cleanAIOutput(rawContent);
				}
				else {
					botReplyBuffer = cleanAIOutput(QString::fromUtf8(fullData));
				}
			}
			else {
				botReplyBuffer = cleanAIOutput(QString::fromUtf8(fullData));
			}
		}
		else {
			qDebug() << "JSON解析错误:" << parseError.errorString();
			botReplyBuffer = cleanAIOutput(QString::fromUtf8(fullData));
		}
	}

	// ✅ 保存AI回复到历史上下文
	chatHistory.append({ "assistant", botReplyBuffer });
	if (chatHistory.size() > MAX_HISTORY)
		chatHistory.removeFirst();

	replyFinished = true;
	reply->deleteLater();

	sendButton->setEnabled(true);
}


void OnlineInferWindow::updateBotReply()
{
	if (botReplyBuffer.isEmpty() && !replyFinished) return;

	// 分段显示回复内容
	const int chunkSize = 12;
	if (bufferPos < botReplyBuffer.length()) {
		const int takeLength = qMin(chunkSize, botReplyBuffer.length() - bufferPos);
		const QString piece = botReplyBuffer.mid(bufferPos, takeLength);
		bufferPos += takeLength;

		textBrowser->moveCursor(QTextCursor::End);
		textBrowser->insertPlainText(piece);
		textBrowser->moveCursor(QTextCursor::End);
	}

	// 回复显示完成后重置状态
	if (replyFinished && bufferPos >= botReplyBuffer.length()) {
		textBrowser->append("");  // 增加空行分隔
		botReplyBuffer.clear();
		bufferPos = 0;
		replyFinished = false;
	}
}

QString OnlineInferWindow::cleanAndUnescape(const QString& raw)
{
	if (raw.isEmpty()) return {};

	QString processed = raw;
	// 移除特殊标签
	processed.remove(QRegularExpression("<\\|.*?\\|>"));

	QString result;
	result.reserve(processed.length());
	const QChar backslash = '\\';

	for (int i = 0; i < processed.length(); ++i) {
		QChar current = processed.at(i);
		if (current == backslash && i + 1 < processed.length()) {
			QChar next = processed.at(i + 1);
			switch (next.toLatin1()) {
			case 'n': result.append('\n'); i++; break;
			case 't': result.append('\t'); i++; break;
			case 'r': result.append('\r'); i++; break;
			case '\\': result.append('\\'); i++; break;
			case '"': result.append('"'); i++; break;
			case '\'': result.append('\''); i++; break;
			case 'x':
				if (i + 3 < processed.length()) {
					bool ok = false;
					ushort hexVal = processed.mid(i + 2, 2).toUShort(&ok, 16);
					if (ok) {
						result.append(QChar(hexVal));
						i += 3;
						break;
					}
				}
				[[fallthrough]];
			case 'u':
				if (i + 5 < processed.length()) {
					bool ok = false;
					ushort hexVal = processed.mid(i + 2, 4).toUShort(&ok, 16);
					if (ok) {
						result.append(QChar(hexVal));
						i += 5;
						break;
					}
				}
				[[fallthrough]];
			default:
				result.append(next);
				i++;
				break;
			}
		}
		else {
			result.append(current);
		}
	}

	// 再次清理可能残留的特殊标签
	result.remove(QRegularExpression("<\\|.*?\\|>"));
	return result;
}

QString OnlineInferWindow::cleanAIOutput(const QString& raw)
{
	QString processed = raw;
	// 移除Markdown标题
	processed.remove(QRegularExpression("^#+\\s.*$", QRegularExpression::MultilineOption));
	// 移除末尾多余的反引号
	processed.remove(QRegularExpression("`+$"));
	// 应用通用清洗规则
	return cleanAndUnescape(processed).trimmed();
}
