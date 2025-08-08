// LLMRunnerYi.cpp  (兼容新版 llama.cpp，无 llama_sample_*，自实现 top-p+temperature)
#include "stdafx.h"
#include "LLMRunnerYi.h"

#include <QMutexLocker>
#include <QDebug>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <limits>
#include <llama.h>

static inline float fast_exp(float x) { return std::exp(x); }

// 简单的本地 top-p + temperature 采样实现（不依赖 llama.cpp 旧采样 API）
static llama_token sample_top_p_temperature(const float* logits,
	int vocab_size,
	float top_p,
	float temperature)
{
	// 1) 温度缩放：logits / T
	std::vector<float> scaled(vocab_size);
	const float invT = (temperature > 0.f) ? (1.0f / temperature) : std::numeric_limits<float>::infinity();
	float max_logit = -std::numeric_limits<float>::infinity();
	for (int i = 0; i < vocab_size; ++i) {
		float v = logits[i] * (std::isfinite(invT) ? invT : 0.f); // T=0时不会用到 softmax，转而走贪心
		scaled[i] = v;
		if (v > max_logit) max_logit = v;
	}

	// T=0 退化为贪心
	if (!(temperature > 0.f)) {
		int best_id = 0;
		float best_val = logits[0];
		for (int j = 1; j < vocab_size; ++j) {
			if (logits[j] > best_val) { best_val = logits[j]; best_id = j; }
		}
		return best_id;
	}

	// 2) 计算 softmax 概率（数值稳定：减去 max）
	double sum = 0.0;
	std::vector<double> probs(vocab_size);
	for (int i = 0; i < vocab_size; ++i) {
		double p = std::exp((double)scaled[i] - (double)max_logit);
		probs[i] = p;
		sum += p;
	}
	if (sum <= 0.0) {
		// 退化处理：全零时，返回最大 logit
		int best_id = 0;
		double best_val = probs[0];
		for (int j = 1; j < vocab_size; ++j) {
			if (probs[j] > best_val) { best_val = probs[j]; best_id = j; }
		}
		return best_id;
	}
	for (int i = 0; i < vocab_size; ++i) probs[i] /= sum;

	// 3) top-p 截断：按概率从大到小排序并保留累计概率>=top_p 的最小集合
	struct Cand { int id; double p; };
	std::vector<Cand> cands; cands.reserve(vocab_size);
	for (int i = 0; i < vocab_size; ++i) cands.push_back({ i, probs[i] });
	std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.p > b.p; });

	double cum = 0.0;
	int cut = 0;
	const double P = std::min(std::max((double)top_p, 1e-5), 1.0);
	for (; cut < (int)cands.size(); ++cut) {
		cum += cands[cut].p;
		if (cum >= P) { cut++; break; }
	}
	if (cut <= 0) cut = std::min(1, (int)cands.size());

	// 4) 在截断后的集合中按相对概率采样
	double sub_sum = 0.0;
	for (int i = 0; i < cut; ++i) sub_sum += cands[i].p;
	double r = (double)QRandomGenerator::global()->generateDouble() * sub_sum;

	double acc = 0.0;
	for (int i = 0; i < cut; ++i) {
		acc += cands[i].p;
		if (r <= acc) return (llama_token)cands[i].id;
	}
	return (llama_token)cands[0].id; // 兜底
}

LLMRunnerYi::LLMRunnerYi(const QString& modelPath, QObject* parent)
	: QObject(parent), m_modelPath(modelPath) {
}

LLMRunnerYi::~LLMRunnerYi() {
	if (m_ctx)   llama_free(m_ctx);
	if (m_model) llama_model_free(m_model);
	llama_backend_free();
}

void LLMRunnerYi::init() {
	llama_backend_init();

	llama_model_params mparams = llama_model_default_params();
	m_model = llama_model_load_from_file(m_modelPath.toUtf8().constData(), mparams);
	if (!m_model) {
		qWarning() << "Failed to load model:" << m_modelPath;
		return;
	}

	llama_context_params cparams = llama_context_default_params();
	cparams.n_ctx = 2048;
	// 可中断
	cparams.abort_callback = [](void* ud)->bool {
		return reinterpret_cast<std::atomic_bool*>(ud)->load();
		};
	cparams.abort_callback_data = &m_abort;

	m_ctx = llama_init_from_model(m_model, cparams);
	if (!m_ctx) {
		qWarning() << "Failed to create context";
		llama_model_free(m_model);
		m_model = nullptr;
		return;
	}

	m_vocab = llama_model_get_vocab(m_model);
	emit signalInitLLMFinished();
}

QString LLMRunnerYi::buildPromptYi(const QString& userMsg) {
	// 放宽：允许 2-3 句
	static const QString kSystem =
		u8"你是一个中文助手。回答尽量简洁，但可以用2-3句把要点说清楚，不要复述用户问题和自我介绍。";

	QString s;
	s += "### System:\n" + kSystem + "\n";
	for (const auto& h : m_history) {
		if (h.first == "User") {
			s += "### User:\n" + h.second + "\n";
		}
		else if (h.first == "Assistant") {
			s += "### Assistant:\n" + h.second + "\n";
		}
	}
	s += "### User:\n" + userMsg + "\n### Assistant:\n";
	return s;
}

QString LLMRunnerYi::chat(const QString& userMsg) {
	QMutexLocker locker(&m_mutex);
	if (!m_model || !m_ctx || !m_vocab) return "模型未加载";

	// 极短问法短路（体验更稳）
	const QString trimmed = userMsg.trimmed();
	if (trimmed == "你是" || trimmed == "你是谁" || trimmed == "你是谁？" || trimmed == "你是？") {
		QString reply = "我是本地离线AI助手（Yi-1.5-6B-Chat）。";
		m_history.append({ "User", userMsg });
		m_history.append({ "Assistant", reply });
		emit chatResult(reply);
		return reply;
	}

	const QString prompt = buildPromptYi(userMsg);
	const std::string input = prompt.toUtf8().constData();

	std::vector<llama_token> tokens(input.size() * 4 + 8);
	int n_tokens = llama_tokenize(
		m_vocab,
		input.c_str(), (int)input.size(),
		tokens.data(), (int)tokens.size(),
		/*add_special*/ true,
		/*parse_special*/ false
	);
	if (n_tokens <= 0) {
		qWarning() << "Tokenize 失败";
		return "输入解析失败";
	}
	tokens.resize(n_tokens);

	// 清 KV，避免历史污染
	llama_memory_clear(llama_get_memory(m_ctx), true);

	// 先喂提示词
	llama_batch batch = llama_batch_init((int)tokens.size(), 0, 1);
	for (int i = 0; i < (int)tokens.size(); ++i) {
		batch.token[i] = tokens[i];
		batch.pos[i] = i;
		batch.n_seq_id[i] = 1;
		batch.seq_id[i][0] = 0;
	}
	batch.n_tokens = (int)tokens.size();
	if (llama_decode(m_ctx, batch) != 0) {
		qWarning() << "llama_decode 提示阶段失败";
	}
	llama_batch_free(batch);

	int n_past = (int)tokens.size();
	const int n_max_new_tokens = 384;           // 更长
	const float top_p = 0.90f;                  // 轻采样
	const float temp = 0.70f;

	QString result;
	int end_punc_count = 0;

	for (int step = 0; step < n_max_new_tokens; ++step) {
		if (m_abort.load()) { qWarning() << "推理被中断"; break; }

		float* logits = llama_get_logits(m_ctx);
		const int vocab_size = llama_vocab_n_tokens(m_vocab);
		if (!logits || vocab_size <= 0) { qWarning() << "logits/vocab 无效"; break; }

		// —— 本地 top-p + temperature 采样 ——
		llama_token new_tok = sample_top_p_temperature(logits, vocab_size, top_p, temp);

		if (new_tok == llama_vocab_eos(m_vocab)) break;

		// 跳过控制/特殊 token 的可见输出（但仍然喂给模型）
		if (!llama_vocab_is_control(m_vocab, new_tok)) {
			char piece[512] = { 0 };
			// 不把特殊 token 渲染成文字
			llama_token_to_piece(m_vocab, new_tok, piece, (int)sizeof(piece), /*lstrip*/0, /*special*/false);
			result += QString::fromUtf8(piece);
		}

		// —— 早停：需要遇到第 2 个句末标点才停 ——
		if (!result.isEmpty()) {
			const QChar tail = result[result.size() - 1];
			if (tail == u'。' || tail == u'！' || tail == u'？' || tail == u'.' || tail == u'!' || tail == u'?') {
				if (++end_punc_count >= 2) break;
			}
		}

		// 如果模型准备开始下一段头，仅当换行后出现 "### " 才截断
		int cutProbe = result.indexOf("\n### ");
		if (cutProbe != -1) break;

		// 继续解码（把新 token 喂回模型）
		llama_batch next = llama_batch_init(1, 0, 1);
		next.token[0] = new_tok;
		next.pos[0] = n_past;
		next.n_seq_id[0] = 1;
		next.seq_id[0][0] = 0;
		next.n_tokens = 1;

		if (llama_decode(m_ctx, next) != 0) {
			qWarning() << "llama_decode 生成阶段失败";
			llama_batch_free(next);
			break;
		}
		llama_batch_free(next);
		++n_past;
	}

	// —— 后处理：只在换行开头出现 "### " 时截断，并清理空白 ——
	QString out = result;
	int cut = out.indexOf("\n### ");
	if (cut != -1) out = out.left(cut);
	out = out.trimmed();

	// 记录历史 & 回传
	m_history.append({ "User", userMsg });
	m_history.append({ "Assistant", out });

	emit chatResult(out);
	return out;
}

void LLMRunnerYi::requestAbort() {
	m_abort.store(true);
}
