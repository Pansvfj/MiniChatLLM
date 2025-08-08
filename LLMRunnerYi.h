#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <atomic>

struct llama_model;
struct llama_context;
struct llama_vocab;

class LLMRunnerYi : public QObject {
	Q_OBJECT
public:
	explicit LLMRunnerYi(const QString& modelPath, QObject* parent = nullptr);
	~LLMRunnerYi();

	void init();                         // 加载模型 & 创建上下文
	QString chat(const QString& userMsg); // 同步推理（也会通过信号发出）

	void requestAbort();                 // 请求中断长回复

signals:
	void chatResult(const QString& result);
	void signalInitLLMFinished();

private:
	QString buildPromptYi(const QString& userMsg); // Yi-Chat 专用模板

private:
	llama_model* m_model = nullptr;
	llama_context* m_ctx = nullptr;
	const llama_vocab* m_vocab = nullptr;

	QString m_modelPath;
	QVector<QPair<QString, QString>> m_history; // {"User"/"Assistant", text}
	QMutex  m_mutex;

	std::atomic_bool m_abort{ false };
};
