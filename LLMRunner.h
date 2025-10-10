#ifndef LLMRUNNER_H
#define LLMRUNNER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <atomic>
#include <llama.h>
#include "simple_rag.hpp"

class LLMRunner : public QObject {
	Q_OBJECT
public:
	enum class Preset {
		GreedyShort = 0,
		Balanced = 1,
	};

	explicit LLMRunner(const QString& modelPath, QObject* parent = nullptr);
	~LLMRunner();

	void init();
	QString chat(const QString& prompt);

	void setPreset(Preset p) { m_preset.store((int)p, std::memory_order_relaxed); }
	Preset preset() const { return (Preset)m_preset.load(std::memory_order_relaxed); }

	void abort() { m_abort.store(true, std::memory_order_relaxed); }
	void requestAbort() { abort(); } // 兼容旧 UI

	void setRag(SimpleRAG* rag) { m_rag = rag; }
	void setEnableAgent(bool on) { m_enableAgent = on; }

	QString buildPromptWithRagAndTools(const QString& userMsg);

signals:
	void tokenArrived(const QString& token);
	void chatResult(const QString& text);
	void errorOccured(const QString& msg);     // 先保留，不强依赖
	void signalInitLLMFinished();              // 兼容旧 UI
	void chatStreamResult(const QString& token);

private:
	QString buildChatML(const QString& userMsg) const;
	llama_sampler* createSamplerForPreset(Preset p) const;

private:
	llama_model* m_model = nullptr;
	llama_context* m_ctx = nullptr;
	const llama_vocab* m_vocab = nullptr;

	QString m_modelPath;
	QMutex  m_mutex;

	std::atomic_bool m_abort{ false };
	std::atomic<int> m_preset{ (int)Preset::Balanced };

	SimpleRAG* m_rag = nullptr;
	bool m_enableAgent = true;
};

#endif // LLMRUNNER_H
