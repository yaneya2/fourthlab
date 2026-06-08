#ifndef FOURTHLAB_FORECAST_CORRECTION_H
#define FOURTHLAB_FORECAST_CORRECTION_H

#include <cmath>
#include <cstddef>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "Ordinal.h"
#include "LazySequence.h"
#include "MutableArraySequence.h"

enum class EventType {
	CpuLoad,
	MemoryLoad,
	Temperature,
	RequestErrors
};

inline std::size_t EventTypeIndex(EventType type) {
	switch (type) {
		case EventType::CpuLoad:
			return 0;
		case EventType::MemoryLoad:
			return 1;
		case EventType::Temperature:
			return 2;
		case EventType::RequestErrors:
			return 3;
	}
	throw std::invalid_argument("Unknown event type");
}

inline std::string EventTypeToString(EventType type) {
	switch (type) {
		case EventType::CpuLoad:
			return "CPU_LOAD";
		case EventType::MemoryLoad:
			return "MEMORY_LOAD";
		case EventType::Temperature:
			return "TEMPERATURE";
		case EventType::RequestErrors:
			return "REQUEST_ERRORS";
	}
	throw std::invalid_argument("Unknown event type");
}

inline EventType EventTypeFromString(const std::string &value) {
	if (value == "CPU_LOAD") {
		return EventType::CpuLoad;
	}
	if (value == "MEMORY_LOAD") {
		return EventType::MemoryLoad;
	}
	if (value == "TEMPERATURE") {
		return EventType::Temperature;
	}
	if (value == "REQUEST_ERRORS") {
		return EventType::RequestErrors;
	}
	throw std::invalid_argument("Unknown event type text: " + value);
}

struct Event {
	std::size_t id = 0;
	long long timestamp = 0;
	EventType type = EventType::CpuLoad;
	double value = 0.0;
	std::string source;
};

struct Prediction {
	bool hasPrediction = false;
	EventType type = EventType::CpuLoad;
	double predictedValue = 0.0;
	double confidence = 0.0;
};

struct Correction {
	bool hasCorrection = false;
	double error = 0.0;
	double absoluteError = 0.0;
	bool isWarning = false;
	bool isCritical = false;
};

enum class ReactionType {
	Normal,
	Warning,
	Critical,
	NoPrediction
};

inline std::string ReactionTypeToString(ReactionType type) {
	switch (type) {
		case ReactionType::Normal:
			return "NORMAL";
		case ReactionType::Warning:
			return "WARNING";
		case ReactionType::Critical:
			return "CRITICAL";
		case ReactionType::NoPrediction:
			return "NO_PREDICTION";
	}
	throw std::invalid_argument("Unknown reaction type");
}

struct Reaction {
	ReactionType type = ReactionType::NoPrediction;
	std::string message;
};

struct ProcessingResult {
	Event currentEvent;
	Prediction previousPrediction;
	Correction correction;
	Reaction reaction;
	Prediction nextPrediction;
};

class HistoryBuffer {
private:
	MutableArraySequence<Event> events;
	std::size_t maxSize;

public:
	explicit HistoryBuffer(std::size_t maxSize = 16) : events(), maxSize(maxSize) {
		if (maxSize == 0) {
			throw std::invalid_argument("History size must be positive");
		}
	}

	void Add(const Event &event) {
		if (events.GetLength() == maxSize) {
			events.Del(0);
		}
		events.Append(event);
	}

	[[nodiscard]] std::size_t GetLength() const {
		return events.GetLength();
	}

	[[nodiscard]] std::size_t GetMaxSize() const {
		return maxSize;
	}

	[[nodiscard]] bool CanPredict(std::size_t minimumCount) const {
		return events.GetLength() >= minimumCount;
	}

	[[nodiscard]] Event Get(std::size_t index) const {
		return events.Get(index);
	}

	Event GetFromEnd(std::size_t offset) const {
		if (offset >= events.GetLength()) {
			throw std::out_of_range("History offset is out of range");
		}
		return events.Get(events.GetLength() - offset - 1);
	}
};

class DifferenceForecastModel {
private:
	std::size_t order;

public:
	explicit DifferenceForecastModel(std::size_t order = 1) : order(order) {
		if (order != 1 && order != 2) {
			throw std::invalid_argument("Forecast order must be 1 or 2");
		}
	}

	[[nodiscard]] std::size_t GetOrder() const {
		return order;
	}

	[[nodiscard]] Prediction PredictNext(const HistoryBuffer &history, EventType type) const {
		std::size_t requiredCount = order + 1;
		if (!history.CanPredict(requiredCount)) {
			return Prediction{false, type, 0.0, 0.0};
		}

		double current = history.GetFromEnd(0).value;
		double previous = history.GetFromEnd(1).value;
		double firstDifference = current - previous;
		double prediction = current + firstDifference;

		if (order == 2) {
			double beforePrevious = history.GetFromEnd(2).value;
			double priorDifference = previous - beforePrevious;
			prediction += firstDifference - priorDifference;
		}

		double confidence = order == 1 ? 0.75 : 0.90;
		return Prediction{true, type, prediction, confidence};
	}
};

class CorrectionService {
private:
	double warningThreshold;
	double criticalThreshold;

public:
	CorrectionService(double warningThreshold, double criticalThreshold)
		: warningThreshold(warningThreshold), criticalThreshold(criticalThreshold) {
		if (warningThreshold < 0.0 || criticalThreshold < warningThreshold) {
			throw std::invalid_argument("Invalid correction thresholds");
		}
	}

	[[nodiscard]] Correction Compare(const Prediction &prediction, const Event &actual) const {
		if (!prediction.hasPrediction || prediction.type != actual.type) {
			return Correction{};
		}

		double error = actual.value - prediction.predictedValue;
		double absoluteError = std::abs(error);
		return Correction{
			true,
			error,
			absoluteError,
			absoluteError >= warningThreshold,
			absoluteError >= criticalThreshold
		};
	}
};

class DecisionMaker {
public:
	static Reaction MakeReaction(const Event &, const Correction &correction) {
		if (!correction.hasCorrection) {
			return Reaction{ReactionType::NoPrediction, "Недостаточно данных для прогноза"};
		}
		if (correction.isCritical) {
			return Reaction{ReactionType::Critical, "Критическое отклонение от прогноза"};
		}
		if (correction.isWarning) {
			return Reaction{ReactionType::Warning, "Значение заметно отклонилось от прогноза"};
		}
		return Reaction{ReactionType::Normal, "Значение находится в пределах прогноза"};
	}
};

class ForecastCorrectionProcessor {
private:
	MutableArraySequence<std::shared_ptr<HistoryBuffer> > histories;
	DifferenceForecastModel model;
	CorrectionService correctionService;
	DecisionMaker decisionMaker;
	MutableArraySequence<Prediction> pendingPredictions;

	void SetPendingPrediction(std::size_t index, const Prediction &prediction) {
		pendingPredictions.Del(index);
		pendingPredictions.InsertAt(prediction, index);
	}

public:
	ForecastCorrectionProcessor(std::size_t order, std::size_t historySize,
	                            double warningThreshold, double criticalThreshold)
		: histories(),
		  model(order),
		  correctionService(warningThreshold, criticalThreshold),
		  decisionMaker(),
		  pendingPredictions{} {
		if (historySize < order + 1) {
			throw std::invalid_argument("History size is too small for forecast order");
		}

		for (std::size_t i = 0; i < 4; ++i) {
			histories.Append(std::shared_ptr<HistoryBuffer>(new HistoryBuffer(historySize)));
			pendingPredictions.Append(Prediction{});
		}
	}

	ProcessingResult Process(const Event &event) {
		std::size_t typeIndex = EventTypeIndex(event.type);
		Prediction previousPrediction = pendingPredictions.Get(typeIndex);
		Correction correction = correctionService.Compare(previousPrediction, event);
		Reaction reaction = DecisionMaker::MakeReaction(event, correction);

		std::shared_ptr<HistoryBuffer> history = histories.Get(typeIndex);
		history->Add(event);
		Prediction nextPrediction = model.PredictNext(*history, event.type);
		SetPendingPrediction(typeIndex, nextPrediction);

		return ProcessingResult{event, previousPrediction, correction, reaction, nextPrediction};
	}

	[[nodiscard]] const HistoryBuffer &GetHistory(EventType type) const {
		return *histories.Get(EventTypeIndex(type));
	}

	[[nodiscard]] Prediction GetPendingPrediction(EventType type) const {
		return pendingPredictions.Get(EventTypeIndex(type));
	}
};

class ProcessingStatistics {
private:
	std::size_t total;
	std::size_t normal;
	std::size_t warning;
	std::size_t critical;
	std::size_t noPrediction;
	double absoluteErrorSum;
	double maximumAbsoluteError;

public:
	ProcessingStatistics()
		: total(0), normal(0), warning(0), critical(0), noPrediction(0),
		  absoluteErrorSum(0.0), maximumAbsoluteError(0.0) {
	}

	void Add(const ProcessingResult &result) {
		++total;
		switch (result.reaction.type) {
			case ReactionType::Normal:
				++normal;
				break;
			case ReactionType::Warning:
				++warning;
				break;
			case ReactionType::Critical:
				++critical;
				break;
			case ReactionType::NoPrediction:
				++noPrediction;
				break;
		}
		if (result.correction.hasCorrection) {
			absoluteErrorSum += result.correction.absoluteError;
			if (result.correction.absoluteError > maximumAbsoluteError) {
				maximumAbsoluteError = result.correction.absoluteError;
			}
		}
	}

	[[nodiscard]] std::size_t GetTotal() const { return total; }
	[[nodiscard]] std::size_t GetNormalCount() const { return normal; }
	[[nodiscard]] std::size_t GetWarningCount() const { return warning; }
	[[nodiscard]] std::size_t GetCriticalCount() const { return critical; }
	[[nodiscard]] std::size_t GetNoPredictionCount() const { return noPrediction; }

	[[nodiscard]] double GetMeanAbsoluteError() const {
		std::size_t corrected = total - noPrediction;
		return corrected == 0 ? 0.0 : absoluteErrorSum / static_cast<double>(corrected);
	}

	[[nodiscard]] double GetMaximumAbsoluteError() const {
		return maximumAbsoluteError;
	}
};

class EventGenerator {
private:
	static Event MakeEvent(std::size_t index, EventType type, double value, const std::string &source) {
		return Event{index + 1, static_cast<long long>(index + 1), type, value, source};
	}

public:
	static std::unique_ptr<LazySequence<Event> > Linear(EventType type, std::size_t count,
	                                                    double firstValue, double step,
	                                                    const std::string& source = "generated") {
		auto rule = [type, firstValue, step, source](std::size_t index) {
			return MakeEvent(index, type, firstValue + step * static_cast<double>(index), source);
		};
		return LazySequence<Event>::FromIndexFunction(rule, Ordinal::Finite(count));
	}

	static std::unique_ptr<LazySequence<Event> > WithSpike(EventType type, std::size_t count,
	                                                       double firstValue, double step,
	                                                       std::size_t spikeIndex, double spikeValue,
	                                                       const std::string& source = "generated") {
		auto rule = [type, firstValue, step, spikeIndex, spikeValue, source](std::size_t index) {
			double value = firstValue + step * static_cast<double>(index);
			if (index == spikeIndex) {
				value = spikeValue;
			}
			return MakeEvent(index, type, value, source);
		};
		return LazySequence<Event>::FromIndexFunction(rule, Ordinal::Finite(count));
	}

	static std::unique_ptr<LazySequence<Event> > Noise(EventType type, std::size_t count,
	                                                   double baseValue, double amplitude,
	                                                   const std::string& source = "generated") {
		auto rule = [type, baseValue, amplitude, source](std::size_t index) {
			int offset = static_cast<int>(index % 5) - 2;
			double value = baseValue + amplitude * static_cast<double>(offset);
			return MakeEvent(index, type, value, source);
		};
		return LazySequence<Event>::FromIndexFunction(rule, Ordinal::Finite(count));
	}
};

inline std::string SerializeEvent(const Event &event) {
	std::ostringstream out;
	out << event.id << ',' << event.timestamp << ',' << EventTypeToString(event.type) << ','
			<< event.value << ',' << event.source;
	return out.str();
}

inline Event DeserializeEvent(const std::string &text) {
	std::istringstream input(text);
	std::string id;
	std::string timestamp;
	std::string type;
	std::string value;
	std::string source;
	if (!std::getline(input, id, ',') || !std::getline(input, timestamp, ',') ||
	    !std::getline(input, type, ',') || !std::getline(input, value, ',') ||
	    !std::getline(input, source)) {
		throw std::invalid_argument("Invalid event CSV line");
	}
	return Event{
		static_cast<std::size_t>(std::stoull(id)),
		std::stoll(timestamp),
		EventTypeFromString(type),
		std::stod(value),
		source
	};
}

inline std::string SerializeProcessingResult(const ProcessingResult &result) {
	std::ostringstream out;
	out << SerializeEvent(result.currentEvent) << ',';
	if (result.previousPrediction.hasPrediction) {
		out << result.previousPrediction.predictedValue;
	} else {
		out << "NONE";
	}
	out << ',';
	if (result.correction.hasCorrection) {
		out << result.correction.error;
	} else {
		out << "NONE";
	}
	out << ',' << ReactionTypeToString(result.reaction.type) << ',';
	if (result.nextPrediction.hasPrediction) {
		out << result.nextPrediction.predictedValue;
	} else {
		out << "NONE";
	}
	return out.str();
}

#endif
