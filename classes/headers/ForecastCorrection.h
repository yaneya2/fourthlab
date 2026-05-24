#ifndef FOURTHLAB_FORECAST_CORRECTION_H
#define FOURTHLAB_FORECAST_CORRECTION_H

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "Cardinal.h"
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
	MutableArraySequence<Event> events_;
	std::size_t maxSize_;

public:
	explicit HistoryBuffer(std::size_t maxSize = 16) : events_(), maxSize_(maxSize) {
		if (maxSize_ == 0) {
			throw std::invalid_argument("History size must be positive");
		}
	}

	void Add(const Event &event) {
		if (events_.GetLength() == maxSize_) {
			events_.Del(0);
		}
		events_.Append(event);
	}

	std::size_t GetLength() const {
		return events_.GetLength();
	}

	std::size_t GetMaxSize() const {
		return maxSize_;
	}

	bool CanPredict(std::size_t minimumCount) const {
		return events_.GetLength() >= minimumCount;
	}

	Event Get(std::size_t index) const {
		return events_.Get(index);
	}

	Event GetFromEnd(std::size_t offset) const {
		if (offset >= events_.GetLength()) {
			throw std::out_of_range("History offset is out of range");
		}
		return events_.Get(events_.GetLength() - offset - 1);
	}
};

class DifferenceForecastModel {
private:
	std::size_t order_;

public:
	explicit DifferenceForecastModel(std::size_t order = 1) : order_(order) {
		if (order_ != 1 && order_ != 2) {
			throw std::invalid_argument("Forecast order must be 1 or 2");
		}
	}

	std::size_t GetOrder() const {
		return order_;
	}

	Prediction PredictNext(const HistoryBuffer &history, EventType type) const {
		std::size_t requiredCount = order_ + 1;
		if (!history.CanPredict(requiredCount)) {
			return Prediction{false, type, 0.0, 0.0};
		}

		double current = history.GetFromEnd(0).value;
		double previous = history.GetFromEnd(1).value;
		double firstDifference = current - previous;
		double prediction = current + firstDifference;

		if (order_ == 2) {
			double beforePrevious = history.GetFromEnd(2).value;
			double priorDifference = previous - beforePrevious;
			prediction += firstDifference - priorDifference;
		}

		double confidence = order_ == 1 ? 0.75 : 0.90;
		return Prediction{true, type, prediction, confidence};
	}
};

class CorrectionService {
private:
	double warningThreshold_;
	double criticalThreshold_;

public:
	CorrectionService(double warningThreshold, double criticalThreshold)
		: warningThreshold_(warningThreshold), criticalThreshold_(criticalThreshold) {
		if (warningThreshold_ < 0.0 || criticalThreshold_ < warningThreshold_) {
			throw std::invalid_argument("Invalid correction thresholds");
		}
	}

	Correction Compare(const Prediction &prediction, const Event &actual) const {
		if (!prediction.hasPrediction || prediction.type != actual.type) {
			return Correction{};
		}

		double error = actual.value - prediction.predictedValue;
		double absoluteError = std::abs(error);
		return Correction{
			true,
			error,
			absoluteError,
			absoluteError >= warningThreshold_,
			absoluteError >= criticalThreshold_
		};
	}
};

class DecisionMaker {
public:
	Reaction MakeReaction(const Event &, const Correction &correction) const {
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
	std::array<HistoryBuffer, 4> histories_;
	DifferenceForecastModel model_;
	CorrectionService correctionService_;
	DecisionMaker decisionMaker_;
	std::array<Prediction, 4> pendingPredictions_;

public:
	ForecastCorrectionProcessor(std::size_t order, std::size_t historySize,
	                            double warningThreshold, double criticalThreshold)
		: histories_{
			  HistoryBuffer(historySize), HistoryBuffer(historySize),
			  HistoryBuffer(historySize), HistoryBuffer(historySize)
		  },
		  model_(order),
		  correctionService_(warningThreshold, criticalThreshold),
		  decisionMaker_(),
		  pendingPredictions_{} {
		if (historySize < order + 1) {
			throw std::invalid_argument("History size is too small for forecast order");
		}
	}

	ProcessingResult Process(const Event &event) {
		std::size_t typeIndex = EventTypeIndex(event.type);
		Prediction previousPrediction = pendingPredictions_[typeIndex];
		Correction correction = correctionService_.Compare(previousPrediction, event);
		Reaction reaction = decisionMaker_.MakeReaction(event, correction);

		histories_[typeIndex].Add(event);
		Prediction nextPrediction = model_.PredictNext(histories_[typeIndex], event.type);
		pendingPredictions_[typeIndex] = nextPrediction;

		return ProcessingResult{event, previousPrediction, correction, reaction, nextPrediction};
	}

	const HistoryBuffer &GetHistory(EventType type) const {
		return histories_[EventTypeIndex(type)];
	}

	Prediction GetPendingPrediction(EventType type) const {
		return pendingPredictions_[EventTypeIndex(type)];
	}
};

class ProcessingStatistics {
private:
	std::size_t total_;
	std::size_t normal_;
	std::size_t warning_;
	std::size_t critical_;
	std::size_t noPrediction_;
	double absoluteErrorSum_;
	double maximumAbsoluteError_;

public:
	ProcessingStatistics()
		: total_(0), normal_(0), warning_(0), critical_(0), noPrediction_(0),
		  absoluteErrorSum_(0.0), maximumAbsoluteError_(0.0) {
	}

	void Add(const ProcessingResult &result) {
		++total_;
		switch (result.reaction.type) {
			case ReactionType::Normal:
				++normal_;
				break;
			case ReactionType::Warning:
				++warning_;
				break;
			case ReactionType::Critical:
				++critical_;
				break;
			case ReactionType::NoPrediction:
				++noPrediction_;
				break;
		}
		if (result.correction.hasCorrection) {
			absoluteErrorSum_ += result.correction.absoluteError;
			if (result.correction.absoluteError > maximumAbsoluteError_) {
				maximumAbsoluteError_ = result.correction.absoluteError;
			}
		}
	}

	std::size_t GetTotal() const { return total_; }
	std::size_t GetNormalCount() const { return normal_; }
	std::size_t GetWarningCount() const { return warning_; }
	std::size_t GetCriticalCount() const { return critical_; }
	std::size_t GetNoPredictionCount() const { return noPrediction_; }

	double GetMeanAbsoluteError() const {
		std::size_t corrected = total_ - noPrediction_;
		return corrected == 0 ? 0.0 : absoluteErrorSum_ / static_cast<double>(corrected);
	}

	double GetMaximumAbsoluteError() const {
		return maximumAbsoluteError_;
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
	                                                    std::string source = "generated") {
		auto rule = [type, firstValue, step, source](std::size_t index) {
			return MakeEvent(index, type, firstValue + step * static_cast<double>(index), source);
		};
		return LazySequence<Event>::FromIndexFunction(rule, Cardinal::Finite(count));
	}

	static std::unique_ptr<LazySequence<Event> > WithSpike(EventType type, std::size_t count,
	                                                       double firstValue, double step,
	                                                       std::size_t spikeIndex, double spikeValue,
	                                                       std::string source = "generated") {
		auto rule = [type, firstValue, step, spikeIndex, spikeValue, source](std::size_t index) {
			double value = firstValue + step * static_cast<double>(index);
			if (index == spikeIndex) {
				value = spikeValue;
			}
			return MakeEvent(index, type, value, source);
		};
		return LazySequence<Event>::FromIndexFunction(rule, Cardinal::Finite(count));
	}

	static std::unique_ptr<LazySequence<Event> > Noise(EventType type, std::size_t count,
	                                                   double baseValue, double amplitude,
	                                                   std::string source = "generated") {
		auto rule = [type, baseValue, amplitude, source](std::size_t index) {
			int offset = static_cast<int>(index % 5) - 2;
			double value = baseValue + amplitude * static_cast<double>(offset);
			return MakeEvent(index, type, value, source);
		};
		return LazySequence<Event>::FromIndexFunction(rule, Cardinal::Finite(count));
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
