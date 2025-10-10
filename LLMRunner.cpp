#include "stdafx.h"
#include "LLMRunner.h"
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <QDebug>
#include "calculator_tool.hpp"

LLMRunner::LLMRunner(const QString& modelPath, QObject* parent)
	: QObject(parent), m_modelPath(modelPath) {
}

LLMRunner::~LLMRunner() {
	if (m_ctx)   llama_free(m_ctx);
	if (m_model) llama_model_free(m_model);
	llama_backend_free();
}

void LLMRunner::init() {
	QMutexLocker locker(&m_mutex);
	if (m_model && m_ctx && m_vocab) return;

	llama_backend_init();

	llama_model_params mparams = llama_model_default_params();
	mparams.vocab_only = false;

	m_model = llama_model_load_from_file(m_modelPath.toUtf8().constData(), mparams);
	if (!m_model) { qWarning() << "加载模型失败"; emit signalInitLLMFinished(); return; }

	llama_context_params cparams = llama_context_default_params();
	cparams.n_ctx = 2048;
	cparams.n_threads = 0; // 让库自己选

	m_ctx = llama_init_from_model(m_model, cparams);
	if (!m_ctx) {
		qWarning() << "创建上下文失败";
		llama_model_free(m_model); m_model = nullptr;
		emit signalInitLLMFinished();
		return;
	}

	m_vocab = llama_model_get_vocab(m_model);

	emit signalInitLLMFinished();
}

// 你的 llama.h 要求用 vocab* 做 token 化
// 兼容部分构建：llama_tokenize 首次调用可能返回负数=所需长度
static inline std::vector<llama_token> tokenize_with_vocab(
	const llama_vocab* vocab, const std::string& text,
	bool add_special = true, bool parse_special = true) {

	int32_t n = llama_tokenize(vocab, text.c_str(), (int32_t)text.size(),
		nullptr, 0, add_special, parse_special);
	if (n == INT32_MIN) return {};        // 极端溢出
	if (n < 0) n = -n;                    // 负数表示所需长度
	if (n <= 0) return {};

	std::vector<llama_token> out(n);
	int32_t n2 = llama_tokenize(vocab, text.c_str(), (int32_t)text.size(),
		out.data(), n, add_special, parse_special);
	if (n2 == INT32_MIN) return {};
	if (n2 < 0) n2 = -n2;
	if (n2 < (int32_t)out.size()) out.resize(n2);
	return out;
}

QString LLMRunner::buildChatML(const QString& userMsg) const {
	static const char* kSys =
		"你是一个严谨的中文助手。"
		"当用户提问算术题时，只输出最终阿拉伯数字，不要解释、不要单位、不要代码。";

	return QString(
		"<|im_start|>system\n%1\n<|im_end|>\n"
		"<|im_start|>user\n%2\n<|im_end|>\n"
		"<|im_start|>assistant\n"
	).arg(QString::fromUtf8(kSys), userMsg);
}

llama_sampler* LLMRunner::createSamplerForPreset(Preset p) const {
	auto sparams = llama_sampler_chain_default_params();
	llama_sampler* smpl = llama_sampler_chain_init(sparams);

	if (p == Preset::GreedyShort) {
		llama_sampler_chain_add(smpl, llama_sampler_init_greedy());
		llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
	}
	else {
		// 你的头文件是 penalties() 版本；top_k 1参；top_p/min_p 2参
		llama_sampler_chain_add(smpl, llama_sampler_init_penalties(
			/*penalty_last_n*/64, /*repeat*/1.05f, /*freq*/0.0f, /*pres*/0.0f));
		llama_sampler_chain_add(smpl, llama_sampler_init_top_k(40));
		llama_sampler_chain_add(smpl, llama_sampler_init_top_p(0.90f, /*min_keep*/1));
		llama_sampler_chain_add(smpl, llama_sampler_init_min_p(0.05f,  /*min_keep*/1));
		llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.70f));
		llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
	}
	return smpl;
}

QString LLMRunner::chat(const QString& userText) {
	QMutexLocker locker(&m_mutex);
	if (!m_model || !m_ctx || !m_vocab) return "模型未加载";

	m_abort.store(false, std::memory_order_relaxed);

	// 1) 显式 ChatML
	const QString promptQ = buildChatML(userText);
	const std::string prompt = promptQ.toUtf8().toStdString();

	// 2) 分词
	std::vector<llama_token> inp =
		tokenize_with_vocab(m_vocab, prompt, /*add_special*/true, /*parse_special*/true);
	if (inp.empty()) {
		qWarning() << "tokenize with specials failed, fallback to plain";
		// 兜底1：仍用 ChatML 文本，但不按 special 解析
		inp = tokenize_with_vocab(m_vocab, prompt, /*add_special*/true, /*parse_special*/false);
	}
	if (inp.empty()) {
		qWarning() << "tokenize plain failed, fallback to minimal prompt";
		// 兜底2：极简提示，确保能跑起来
		const std::string plain = (userText + "\n答：").toUtf8().toStdString();
		inp = tokenize_with_vocab(m_vocab, plain, /*add_special*/true, /*parse_special*/false);
	}
	if (inp.empty()) return "tokenize 失败";

	// 3) 清空记忆（替换掉不存在的 llama_kv_cache_clear）
	{
		llama_memory_t mem = llama_get_memory(m_ctx);
		llama_memory_clear(mem, /*data=*/true);
	}

	// 4) 提示阶段
	const llama_seq_id seq_id = 0;
	llama_batch batch = llama_batch_init(
		(int32_t)std::max<int>(512, (int)inp.size() + 256), 0, 1);

	for (int i = 0; i < (int)inp.size(); ++i) {
		batch.token[i] = inp[i];
		batch.pos[i] = i;
		batch.n_seq_id[i] = 1;
		batch.seq_id[i][0] = seq_id;
		batch.logits[i] = (i == (int)inp.size() - 1);
	}
	batch.n_tokens = (int32_t)inp.size();

	if (llama_decode(m_ctx, batch) != 0) {
		llama_batch_free(batch);
		qWarning() << "llama_decode 失败（提示阶段）";
		return "解码失败";
	}

	// 5) 结束标记
	llama_token tok_im_end = -1;
	{
		auto v = tokenize_with_vocab(m_vocab, std::string("<|im_end|>"),
			/*add_special*/true, /*parse_special*/true);
		if (v.size() == 1) tok_im_end = v[0];
	}

	// 6) 采样器
	const Preset p = preset();
	llama_sampler* smpl = createSamplerForPreset(p);

	// 7) 生成循环
	QString out;
	int n_past = (int)inp.size();
	const int n_max_new = (p == Preset::GreedyShort) ? 120 : 256;
	const llama_token eos = llama_vocab_eos(m_vocab);

	for (int n = 0; n < n_max_new && !m_abort.load(std::memory_order_relaxed); ++n) {
		llama_token id = llama_sampler_sample(smpl, m_ctx, -1);
		llama_sampler_accept(smpl, id);

		if (id == eos || (tok_im_end != -1 && id == tok_im_end)) break;

		// detokenize（首参是 vocab）
		char buf[512];
		int32_t nw = llama_token_to_piece(
			m_vocab, id, buf, (int32_t)sizeof(buf),
			/*lstrip*/0, /*special*/false);
		if (nw > 0) {
			QString piece = QString::fromUtf8(buf, nw);
			if (!piece.startsWith("<|")) {
				out += piece;
				emit tokenArrived(piece);
				emit chatStreamResult(piece);
			}
		}

		// 继续解码（单 token）
		batch.token[0] = id;
		batch.pos[0] = n_past;
		batch.n_seq_id[0] = 1;
		batch.seq_id[0][0] = seq_id;
		batch.logits[0] = true;
		batch.n_tokens = 1;

		if (llama_decode(m_ctx, batch) != 0) {
			qWarning() << "llama_decode 失败（生成阶段）";
			break;
		}
		++n_past;
	}

	llama_batch_free(batch);
	llama_sampler_free(smpl);

	out = out.trimmed();
	emit chatResult(out);
	return out;
}

QString LLMRunner::buildPromptWithRagAndTools(const QString& userMsg) {
	QString retrieved;
	if (m_rag) retrieved = m_rag->retrieve(userMsg);

	QString toolResult;
	QRegExp calcRx("\\d+[\\s]*[\\+\\-\\*\\/]\\s*\\d+");
	if (userMsg.contains(calcRx)) {
		toolResult = CalculatorTool::calculate(userMsg);
	}

	QString systemPrompt;
	if (!retrieved.isEmpty())
		systemPrompt += QString("参考文档片段：\n%1\n\n").arg(retrieved);
	if (!toolResult.isEmpty())
		systemPrompt += QString("[工具: calculator] 结果：%1\n\n").arg(toolResult);
	if (systemPrompt.isEmpty())
		systemPrompt = QStringLiteral("你是助手，回答时尽量准确并附带来源（如果有）。");

	QString prompt = systemPrompt + "\n用户: " + userMsg + "\n助手:";
	return prompt;
}
