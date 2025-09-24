#include "OnlineInfetWindow.h"
#include <QNetworkRequest>
#include <QTextBrowser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QDebug>

OnlineInfetWindow::OnlineInfetWindow(const QString& url, QWidget* parent)
	: QWidget(parent),
	manager(new QNetworkAccessManager(this)),
	serverUrl(url),
	bufferPos(0),
	replyFinished(false)
{
	QTextBrowser* textBrowser = new QTextBrowser(this);
	textBrowser->setObjectName("textBrowser");
	textBrowser->setReadOnly(true);

	QLineEdit* lineEdit = new QLineEdit(this);
	lineEdit->setObjectName("lineEdit");

	QPushButton* sendButton = new QPushButton("Send", this);
	sendButton->setObjectName("sendButton");

	QVBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(textBrowser);

	QHBoxLayout* inputLayout = new QHBoxLayout;
	inputLayout->addWidget(lineEdit);
	inputLayout->addWidget(sendButton);

	mainLayout->addLayout(inputLayout);
	setLayout(mainLayout);

	connect(sendButton, &QPushButton::clicked, this, &OnlineInfetWindow::onSendButtonClicked);
	connect(lineEdit, &QLineEdit::returnPressed, sendButton, &QPushButton::click);

	// 定时器按块更新显示（不是每字符 append，使用 insertPlainText）
	displayTimer = new QTimer(this);
	connect(displayTimer, &QTimer::timeout, this, &OnlineInfetWindow::updateBotReply);
	displayTimer->start(40); // 每40ms尝试显示新内容

	lineEdit->setFocus();
}

void OnlineInfetWindow::onSendButtonClicked()
{
	QLineEdit* lineEdit = findChild<QLineEdit*>("lineEdit");
	QString text = lineEdit->text().trimmed();
	if (text.isEmpty()) return;

	QTextBrowser* textBrowser = findChild<QTextBrowser*>("textBrowser");
	textBrowser->append("<b>You:</b> " + text);
	lineEdit->clear();

	// 重置缓冲状态
	botReplyBuffer.clear();
	bufferPos = 0;
	replyFinished = false;

	// 在 UI 中插入 AI 段落标签（后续使用 insertPlainText 追加）
	textBrowser->append("<b>AI:</b> ");

	// 发送请求
	sendRequest(text);
}

void OnlineInfetWindow::sendRequest(const QString& text)
{
	QJsonObject jsonRequest;
	QJsonArray messagesArray;
	QJsonObject message;
	message["role"] = "user";
	message["content"] = text;
	messagesArray.append(message);
	jsonRequest["messages"] = messagesArray;

	// 把 max_tokens 提高，保证能生成更长文本（视模型能力决定）
	jsonRequest["max_tokens"] = 4096;

	QJsonDocument jsonDoc(jsonRequest);
	QByteArray requestData = jsonDoc.toJson(QJsonDocument::Compact);

	QNetworkRequest request;
	request.setUrl(QUrl(serverUrl));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply* reply = manager->post(request, requestData);

	// 初始化累积缓冲
	replyBufferMap.insert(reply, QByteArray());

	// 绑定 readyRead（用于累积）和 finished（最终完成）
	connect(reply, &QNetworkReply::readyRead, this, &OnlineInfetWindow::onReplyReadyRead);
	connect(reply, &QNetworkReply::finished, this, &OnlineInfetWindow::onReplyFinished);

	qDebug() << "Request sent (len=" << requestData.size() << "):" << requestData.left(512);
}

void OnlineInfetWindow::onReplyReadyRead()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) return;

	QByteArray chunk = reply->readAll();
	if (chunk.isEmpty()) return;

	// 仅把 chunk 累积到对应 reply 的 buffer 中，不尝试单独解析
	replyBufferMap[reply].append(chunk);

	// 可选：为了调试，打印前面一小段
	qDebug() << "readyRead chunk len=" << chunk.size() << " preview:" << chunk.left(256);
}

void OnlineInfetWindow::onReplyFinished()
{
	QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
	if (!reply) return;

	// 取出累积的整个响应 body（readyRead 期间累积）
	QByteArray full = replyBufferMap.value(reply);
	if (full.isEmpty()) full = reply->readAll();
	replyBufferMap.remove(reply);

	if (reply->error() == QNetworkReply::NoError) {
		qDebug() << "Server Response (full len=" << full.size() << "):" << full.left(2048);

		QJsonParseError err;
		QJsonDocument jsonDoc = QJsonDocument::fromJson(full, &err);
		if (err.error == QJsonParseError::NoError && jsonDoc.isObject()) {
			QJsonObject jsonResponse = jsonDoc.object();

			if (jsonResponse.contains("choices")) {
				QJsonArray choices = jsonResponse.value("choices").toArray();
				if (!choices.isEmpty()) {
					// 处理第一项，兼容多种结构
					QJsonValue first = choices.at(0);
					QString raw;

					if (first.isObject()) {
						QJsonObject choiceObj = first.toObject();

						// 优先 text 字段（有些实现使用 text）
						if (choiceObj.contains("text")) {
							raw = choiceObj.value("text").toString();
						}
						// 再尝试 message.content（OpenAI-style）
						else if (choiceObj.contains("message")) {
							QJsonValue msgVal = choiceObj.value("message");
							if (msgVal.isObject()) {
								QJsonObject msgObj = msgVal.toObject();
								raw = msgObj.value("content").toString();
							}
							else if (msgVal.isString()) {
								raw = msgVal.toString();
							}
						}
						// 兼容其他可能的字段
						else {
							// 尝试把整个 choiceObj 转成字符串（备选）
							raw = QJsonDocument(choiceObj).toJson(QJsonDocument::Compact);
						}
					}
					else if (first.isString()) {
						raw = first.toString();
					}
					else {
						raw = QString::fromUtf8(full); // 兜底
					}

					botReplyBuffer = cleanAndUnescape(raw);
				}
				else {
					qDebug() << "No choices in response JSON.";
					botReplyBuffer = cleanAndUnescape(QString::fromUtf8(full));
				}
			}
			else {
				// 非标准结构，兜底尝试直接把 body 解码为文本
				botReplyBuffer = cleanAndUnescape(QString::fromUtf8(full));
			}
		}
		else {
			// 不是 JSON（可能是纯文本或流式），当做文本处理
			qDebug() << "JSON parse error:" << err.errorString() << ", treating full body as text.";
			botReplyBuffer = cleanAndUnescape(QString::fromUtf8(full));
		}
	}
	else {
		qDebug() << "Network error:" << reply->errorString();
		QByteArray resp = full;
		qDebug() << "Error response body:" << resp.left(1024);
		QTextBrowser* textBrowser = findChild<QTextBrowser*>("textBrowser");
		textBrowser->append("<span style='color:red;'><b>AI Error:</b> " + reply->errorString() + "</span>");
		botReplyBuffer = QString("\n[Error body]\n") + QString::fromUtf8(resp);
	}

	// 标记网络层完成；显示器的定时器会把缓冲显示出来并在显示完毕时认为“结束”
	replyFinished = true;
	reply->deleteLater();
}


void OnlineInfetWindow::updateBotReply()
{
	// 若无内容且网络还没完成，则不显示
	if (botReplyBuffer.isEmpty() && !replyFinished) return;

	// 每次追加一小块，避免频繁 UI 操作（可调整 chunkSize）
	const int chunkSize = 12;
	if (bufferPos < botReplyBuffer.length()) {
		int take = qMin(chunkSize, botReplyBuffer.length() - bufferPos);
		QString piece = botReplyBuffer.mid(bufferPos, take);
		bufferPos += take;

		QTextBrowser* textBrowser = findChild<QTextBrowser*>("textBrowser");
		textBrowser->moveCursor(QTextCursor::End);
		textBrowser->insertPlainText(piece);
		textBrowser->moveCursor(QTextCursor::End);
	}

	// 当网络已结束且缓冲已全部显示 => 回复完全结束
	if (replyFinished && bufferPos >= botReplyBuffer.length()) {
		QTextBrowser* textBrowser = findChild<QTextBrowser*>("textBrowser");
		textBrowser->append(""); // 添加换行段落，准备下一条输入

		qDebug() << "Bot reply fully received and displayed.";
		// 重置状态，准备下一次会话（不清除 UI）
		botReplyBuffer.clear();
		bufferPos = 0;
		replyFinished = false;
	}
}

// 清洗并解码：移除 <|...|> 标签并把常见转义序列 (\xHH, \uXXXX, \n, \t, \\) 解码为真实字符
QString OnlineInfetWindow::cleanAndUnescape(const QString& raw)
{
	if (raw.isEmpty()) return QString();

	// 1) 移除形如 <|...|> 的标记（非贪婪）
	QString s = raw;
	QRegularExpression tagRe("<\\|.*?\\|>");
	s.remove(tagRe);

	// 2) 移除常见多余分隔符（如果有）例如 leftover markers like `---` or similar (可选)
	// s.remove(QRegularExpression("[-]{3,}"));

	// 3) 处理 C 风格转义：遍历并转换 \n \t \\ \xHH \uHHHH 等
	QString out;
	out.reserve(s.length());

	const QChar backslash = '\\';
	for (int i = 0; i < s.length(); ++i) {
		QChar c = s.at(i);
		if (c == backslash && i + 1 < s.length()) {
			QChar n = s.at(i + 1);
			if (n == 'n') { out.append('\n'); i += 1; continue; }
			if (n == 't') { out.append('\t'); i += 1; continue; }
			if (n == 'r') { out.append('\r'); i += 1; continue; }
			if (n == '\\') { out.append('\\'); i += 1; continue; }
			if (n == '"') { out.append('"'); i += 1; continue; }
			if (n == '\'') { out.append('\''); i += 1; continue; }

			// \xHH
			if (n == 'x' && i + 3 < s.length()) {
				bool ok = true;
				QString hex = s.mid(i + 2, 2);
				uchar byte = static_cast<uchar>(hex.toUInt(&ok, 16));
				if (ok) {
					// 以 UTF-8 方式将单字节插入
					QByteArray b;
					b.append(static_cast<char>(byte));
					out += QString::fromUtf8(b);
					i += 3;
					continue;
				}
			}

			// \uHHHH
			if (n == 'u' && i + 5 < s.length()) {
				QString hex = s.mid(i + 2, 4);
				bool ok = false;
				uint code = hex.toUInt(&ok, 16);
				if (ok) {
					out.append(QChar(static_cast<ushort>(code)));
					i += 5;
					continue;
				}
			}

			// 默认：跳过反斜杠，保留后续字符
			out.append(n);
			i += 1;
			continue;
		}
		else {
			out.append(c);
		}
	}

	// 4) 最后再做一次去除可能残留的 tags（防万一）
	out.remove(QRegularExpression("<\\|.*?\\|>"));

	return out;
}
