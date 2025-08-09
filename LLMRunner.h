#ifndef LLMRUNNER_H
#define LLMRUNNER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <atomic>
#include <llama.h>

class LLMRunner : public QObject {
	Q_OBJECT
public:
	explicit LLMRunner(const QString& modelPath, QObject* parent = nullptr);
	~LLMRunner();

	void init();

	// 同步接口：会在工作线程里被 QtConcurrent::run 调用
	// 内部会一边生成一边通过 chatStreamResult() 往外推
	QString chat(const QString& prompt);

	// 请求中止当前生成
	void requestAbort() { m_abort.store(true); }

signals:
	// 流式分片：每次生成出一个 piece 就发一次
	void chatStreamResult(const QString& partialResult);

	// 生成完成（或被中断）后的最终文本
	void chatResult(const QString& result);

	// 模型初始化完成
	void signalInitLLMFinished();

private:
	QString buildPrompt(const QString& prompt);

private:
	llama_model* m_model = nullptr;
	llama_context* m_ctx = nullptr;
	const llama_vocab* m_vocab = nullptr;

	QString m_modelPath;
	QVector<QPair<QString, QString>> m_history;
	QMutex m_mutex;

	std::atomic_bool m_abort{ false };
};

#endif // LLMRUNNER_H
