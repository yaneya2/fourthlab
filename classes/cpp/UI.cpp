#include "UI.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include "ForecastCorrection.h"
#include "LazySequence.h"
#include "MutableArraySequence.h"
#include "Streams.h"

namespace {
	using Number = long long;
	using Lazy = LazySequence<Number>;

	void ClearInput() {
		std::cin.clear();
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}

	template<class T>
	T ReadValue(const std::string &prompt) {
		while (true) {
			std::cout << prompt;
			T value{};
			if (std::cin >> value) {
				return value;
			}
			ClearInput();
			std::cout << "Некорректный ввод. Повторите попытку.\n";
		}
	}

	std::string ReadLine(const std::string &prompt) {
		std::cout << prompt;
		std::string value;
		std::getline(std::cin >> std::ws, value);
		return value;
	}

	Ordinal ReadOrdinal(const std::string &prompt) {
		std::cout << prompt << "\n";
		auto omegaCoefficient = ReadValue<std::size_t>("Коэффициент при omega: ");
		auto finitePart = ReadValue<std::size_t>("Конечная часть: ");
		return Ordinal::FromParts(omegaCoefficient, finitePart);
	}

	Ordinal ReadLength() {
		std::cout << "1. Конечная длина\n";
		std::cout << "2. Бесконечная длина (омега)\n";
		int choice = ReadValue<int>("Выбор: ");
		if (choice == 2) {
			return Ordinal::Omega();
		}
		return Ordinal::Finite(ReadValue<std::size_t>("Длина: "));
	}

	MutableArraySequence<Number> ReadItems() {
		MutableArraySequence<Number> result;
		auto count = ReadValue<std::size_t>("Количество элементов: ");
		for (std::size_t i = 0; i < count; ++i) {
			result.Append(ReadValue<Number>("Элемент[" + std::to_string(i) + "] = "));
		}
		return result;
	}

	std::unique_ptr<Lazy> CreateFiniteLazy() {
		MutableArraySequence<Number> source = ReadItems();
		return std::make_unique<Lazy>(static_cast<const Sequence<Number> &>(source));
	}

	template<class T>
	T SequenceAt(Sequence<T> *sequence, std::size_t index) {
		std::unique_ptr<IEnumerator<T> > enumerator(sequence->GetEnumerator());
		for (std::size_t i = 0; i <= index; ++i) {
			if (!enumerator->MoveNext()) {
				throw std::out_of_range("Индекс вне диапазона");
			}
		}
		return enumerator->Current();
	}

	Number FibonacciRule(Sequence<Number> *prefix) {
		std::size_t length = prefix->GetLength();
		if (length < 2) {
			throw std::logic_error("Для чисел Фибоначчи нужны два начальных элемента");
		}
		return SequenceAt(prefix, length - 1) + SequenceAt(prefix, length - 2);
	}

	std::unique_ptr<Lazy> CreateByIndexRule(std::function<Number(std::size_t)> rule) {
		Ordinal length = ReadLength();
		if (length.IsInfinite()) {
			return Lazy::Infinite(std::move(rule));
		}
		return Lazy::FromIndexFunction(std::move(rule), length);
	}

	std::unique_ptr<Lazy> CreateFibonacciLazy() {
		Number seedsData[] = {0, 1};
		MutableArraySequence<Number> seeds(seedsData, 2);
		return std::make_unique<Lazy>(FibonacciRule, &seeds, ReadLength());
	}

	std::unique_ptr<Lazy> CreateArithmeticProgression() {
		auto first = ReadValue<Number>("Первый элемент: ");
		auto step = ReadValue<Number>("Шаг: ");
		return CreateByIndexRule([first, step](std::size_t index) {
			return first + step * static_cast<Number>(index);
		});
	}

	std::unique_ptr<Lazy> CreateGeometricProgression() {
		auto first = ReadValue<Number>("Первый элемент: ");
		auto ratio = ReadValue<Number>("Знаменатель прогрессии: ");
		MutableArraySequence<Number> seeds(&first, 1);
		auto rule = [ratio](Sequence<Number> *prefix) {
			std::size_t length = prefix->GetLength();
			return SequenceAt(prefix, length - 1) * ratio;
		};
		return std::make_unique<Lazy>(std::function<Number(Sequence<Number> *)>(rule), &seeds, ReadLength());
	}

	std::unique_ptr<Lazy> CreateSquares() {
		return CreateByIndexRule([](std::size_t index) {
			auto n = static_cast<Number>(index + 1);
			return n * n;
		});
	}

	std::unique_ptr<Lazy> CreateCubes() {
		return CreateByIndexRule([](std::size_t index) {
			auto n = static_cast<Number>(index + 1);
			return n * n * n;
		});
	}

	std::unique_ptr<Lazy> CreateFactorials() {
		Number first = 1;
		MutableArraySequence<Number> seeds(&first, 1);
		auto rule = [](Sequence<Number> *prefix) {
			std::size_t length = prefix->GetLength();
			return SequenceAt(prefix, length - 1) * static_cast<Number>(length + 1);
		};
		return std::make_unique<Lazy>(std::function<Number(Sequence<Number> *)>(rule), &seeds, ReadLength());
	}

	std::unique_ptr<Lazy> CreateLinearFormula() {
		auto a = ReadValue<Number>("a = ");
		auto b = ReadValue<Number>("b = ");
		return CreateByIndexRule([a, b](std::size_t index) {
			return a * static_cast<Number>(index) + b;
		});
	}

	std::unique_ptr<Lazy> CreateNaturalNumbers() {
		return CreateByIndexRule([](std::size_t index) {
			return static_cast<Number>(index + 1);
		});
	}

	std::unique_ptr<Lazy> CreateGeneratedLazy() {
		std::cout << "\nГенератор ленивой последовательности\n";
		std::cout << "1. Числа Фибоначчи\n";
		std::cout << "2. Арифметическая прогрессия\n";
		std::cout << "3. Геометрическая прогрессия\n";
		std::cout << "4. Квадраты натуральных чисел\n";
		std::cout << "5. Кубы натуральных чисел\n";
		std::cout << "6. Факториалы\n";
		std::cout << "7. Линейная формула a*n + b\n";
		std::cout << "8. Натуральные числа\n";
		switch (ReadValue<int>("Выбор: ")) {
			case 1:
				return CreateFibonacciLazy();
			case 2:
				return CreateArithmeticProgression();
			case 3:
				return CreateGeometricProgression();
			case 4:
				return CreateSquares();
			case 5:
				return CreateCubes();
			case 6:
				return CreateFactorials();
			case 7:
				return CreateLinearFormula();
			case 8:
				return CreateNaturalNumbers();
			default:
				throw std::invalid_argument("Неизвестный генератор");
		}
	}

	std::unique_ptr<Lazy> CreateLazyOperand() {
		std::cout << "\nПоследовательность для операции\n";
		std::cout << "1. Ввести конечную последовательность\n";
		std::cout << "2. Создать через генератор\n";
		int choice = ReadValue<int>("Выбор: ");
		if (choice == 1) {
			return CreateFiniteLazy();
		}
		if (choice == 2) {
			return CreateGeneratedLazy();
		}
		throw std::invalid_argument("Неизвестный тип последовательности");
	}

	void PrintFiniteSequence(const Sequence<Number> &sequence) {
		std::unique_ptr<IEnumerator<Number> > enumerator(sequence.GetEnumerator());
		std::cout << "Элементы: ";
		bool first = true;
		while (enumerator->MoveNext()) {
			if (!first) {
				std::cout << ' ';
			}
			std::cout << enumerator->Current();
			first = false;
		}
		std::cout << "\nДлина: " << sequence.GetLength() << "\n";
	}

	void MapCurrent(std::unique_ptr<Lazy> &sequence) {
		std::cout << "1. Возвести в квадрат\n";
		std::cout << "2. Умножить на число\n";
		std::cout << "3. Применить a*x + b\n";
		int choice = ReadValue<int>("Выбор: ");
		if (choice == 1) {
			sequence = sequence->Map<Number>([](Number x) { return x * x; });
		} else if (choice == 2) {
			auto multiplier = ReadValue<Number>("Множитель: ");
			sequence = sequence->Map<Number>([multiplier](Number x) { return x * multiplier; });
		} else if (choice == 3) {
			auto a = ReadValue<Number>("a = ");
			auto b = ReadValue<Number>("b = ");
			sequence = sequence->Map<Number>([a, b](Number x) { return a * x + b; });
		} else {
			throw std::invalid_argument("Неизвестное преобразование");
		}
	}

	void FilterCurrent(std::unique_ptr<Lazy> &sequence) {
		std::cout << "1. Только чётные\n";
		std::cout << "2. Только нечётные\n";
		std::cout << "3. Больше указанного значения\n";
		std::cout << "4. Кратные указанному числу\n";
		int choice = ReadValue<int>("Выбор: ");
		if (choice == 1) {
			sequence = sequence->Where([](Number x) { return x % 2 == 0; });
		} else if (choice == 2) {
			sequence = sequence->Where([](Number x) { return x % 2 != 0; });
		} else if (choice == 3) {
			auto bound = ReadValue<Number>("Граница: ");
			sequence = sequence->Where([bound](Number x) { return x > bound; });
		} else if (choice == 4) {
			auto divisor = ReadValue<Number>("Делитель: ");
			if (divisor == 0) {
				throw std::invalid_argument("Делитель не может быть нулём");
			}
			sequence = sequence->Where([divisor](Number x) { return x % divisor == 0; });
		} else {
			throw std::invalid_argument("Неизвестный фильтр");
		}
	}

	void ReduceCurrent(const Lazy &sequence, bool firstN) {
		std::cout << "1. Сумма\n";
		std::cout << "2. Произведение\n";
		std::cout << "3. Максимум\n";
		int choice = ReadValue<int>("Выбор: ");
		std::size_t count = firstN ? ReadValue<std::size_t>("Количество первых элементов: ") : 0;
		Number initial = choice == 2 ? 1 : std::numeric_limits<Number>::lowest();
		if (choice == 1) {
			initial = 0;
			auto reducer = std::function<Number(Number, Number)>([](Number acc, Number x) { return acc + x; });
			Number result = firstN ? sequence.ReduceFirstN(count, initial, reducer) : sequence.Reduce(initial, reducer);
			std::cout << "Сумма: " << result << "\n";
		} else if (choice == 2) {
			auto reducer = std::function<Number(Number, Number)>([](Number acc, Number x) { return acc * x; });
			Number result = firstN ? sequence.ReduceFirstN(count, initial, reducer) : sequence.Reduce(initial, reducer);
			std::cout << "Произведение: " << result << "\n";
		} else if (choice == 3) {
			auto reducer = std::function<Number(Number, Number)>([](Number acc, Number x) {
				return acc > x ? acc : x;
			});
			Number result = firstN ? sequence.ReduceFirstN(count, initial, reducer) : sequence.Reduce(initial, reducer);
			std::cout << "Максимум: " << result << "\n";
		} else {
			throw std::invalid_argument("Неизвестная свёртка");
		}
	}

	void EnumerateCurrent(const Lazy &sequence) {
		auto count = ReadValue<std::size_t>("Сколько элементов перечислить: ");
		auto enumerator = sequence.GetEnumerator();
		std::cout << "Перечислитель: ";
		for (std::size_t i = 0; i < count && enumerator->MoveNext(); ++i) {
			std::cout << enumerator->Current() << ' ';
		}
		std::cout << "\nСброс перечислителя. Первый элемент после Reset: ";
		enumerator->Reset();
		if (enumerator->MoveNext()) {
			std::cout << enumerator->Current() << "\n";
		} else {
			std::cout << "последовательность пуста\n";
		}
	}

	void LazyWorkspace(std::unique_ptr<Lazy> sequence) {
		while (true) {
			std::cout << "\nОперации над текущей LazySequence\n";
			std::cout << "1. Показать длину и размер кэша\n2. GetFirst / GetLast\n";
			std::cout << "3. Get по индексу\n4. Конечная подпоследовательность\n";
			std::cout << "5. Concat с последовательностью\n6. Вставить последовательность\n";
			std::cout << "7. Map\n8. Where\n9. Reduce всей последовательности\n10. ReduceFirstN\n";
			std::cout << "11. Обход через GetEnumerator\n12. Append последовательности\n13. Prepend последовательности\n0. Назад\n";
			int choice = ReadValue<int>("Выбор: ");
			if (choice == 0) {
				return;
			}
			try {
				if (choice == 1) {
					std::cout << "Длина: " << sequence->GetLength().ToString() << "\n";
					std::cout << "Материализовано: " << sequence->GetMaterializedCount() << "\n";
				} else if (choice == 2) {
					std::cout << "Первый элемент: " << sequence->GetFirst() << "\n";
					std::cout << "Последний элемент: " << sequence->GetLast() << "\n";
				} else if (choice == 3) {
					Ordinal index = ReadOrdinal("Индекс");
					std::cout << "Значение: " << sequence->Get(index) << "\n";
				} else if (choice == 4) {
					auto start = ReadValue<std::size_t>("Начальный индекс: ");
					auto end = ReadValue<std::size_t>("Конечный индекс: ");
					sequence = sequence->GetSubsequence(Ordinal::Finite(start), Ordinal::Finite(end));
					std::cout << "Подпоследовательность стала текущей.\n";
				}else if (choice == 5) {
					auto second = CreateLazyOperand();
					sequence = sequence->Concat(*second);
				} else if (choice == 6) {
					auto inserted = CreateLazyOperand();
					Ordinal index = ReadOrdinal("Индекс вставки");
					sequence = sequence->InsertAt(*inserted, index);
				} else if (choice == 7) {
					MapCurrent(sequence);
				} else if (choice == 8) {
					FilterCurrent(sequence);
				} else if (choice == 9) {
					ReduceCurrent(*sequence, false);
				} else if (choice == 10) {
					ReduceCurrent(*sequence, true);
				}else if (choice == 11) {
					EnumerateCurrent(*sequence);
				} else if (choice == 12) {
					auto appended = CreateLazyOperand();
					sequence = sequence->Append(*appended);
				} else if (choice == 13) {
					auto prepended = CreateLazyOperand();
					sequence = sequence->Prepend(*prepended);
				} else {
					std::cout << "Неизвестная команда.\n";
				}
			} catch (const std::exception &error) {
				std::cout << "Ошибка операции: " << error.what() << "\n";
			}
		}
	}

	void PrintReadStreamStatus(const ReadOnlyStream<Number> &stream) {
		std::cout << "Позиция: " << stream.GetPosition();
		std::cout << ", конец потока: " << (stream.IsEndOfStream() ? "да" : "нет");
		std::cout << ", поддерживает Seek: " << (stream.IsCanSeek() ? "да" : "нет");
		std::cout << ", поддерживает возврат: " << (stream.IsCanGoBack() ? "да" : "нет") << "\n";
	}

	template<class Stream>
	void ReadStreamCommands(Stream &stream) {
		while (true) {
			std::cout << "\n1. Open\n2. Read\n3. Seek\n4. Состояние\n5. Close\n0. Назад\n";
			int choice = ReadValue<int>("Выбор: ");
			if (choice == 0) {
				return;
			}
			try {
				if (choice == 1) {
					stream.Open();
					std::cout << "Поток открыт.\n";
				} else if (choice == 2) {
					std::cout << "Прочитано: " << stream.Read() << "\n";
				} else if (choice == 3) {
					std::cout << "Новая позиция: " << stream.Seek(ReadValue<std::size_t>("Позиция: ")) << "\n";
				} else if (choice == 4) {
					PrintReadStreamStatus(stream);
				} else if (choice == 5) {
					stream.Close();
					std::cout << "Поток закрыт.\n";
				}
			} catch (const std::exception &error) {
				std::cout << "Ошибка потока: " << error.what() << "\n";
			}
		}
	}

	void SequenceReadPanel() {
		MutableArraySequence<Number> source = ReadItems();
		SequenceReadStream<Number> stream(source);
		ReadStreamCommands(stream);
	}

	void LazyReadPanel() {
		std::cout << "1. Ввести конечную последовательность\n2. Создать через генератор\n";
		int choice = ReadValue<int>("Выбор: ");
		auto source = choice == 1 ? CreateFiniteLazy() : CreateGeneratedLazy();
		LazySequenceReadStream<Number> stream(*source);
		while (true) {
			std::cout << "\nПоток чтения LazySequence\n";
			std::cout << "1. Open\n2. Read\n3. Seek\n4. Состояние и кэш\n5. Close\n0. Назад\n";
			int command = ReadValue<int>("Выбор: ");
			if (command == 0) {
				return;
			}
			try {
				if (command == 1) {
					stream.Open();
				} else if (command == 2) {
					std::cout << "Прочитано: " << stream.Read() << "\n";
				} else if (command == 3) {
					std::cout << "Новая позиция: " << stream.Seek(ReadValue<std::size_t>("Позиция: ")) << "\n";
				} else if (command == 4) {
					PrintReadStreamStatus(stream);
					std::cout << "Материализовано в источнике: " << source->GetMaterializedCount() << "\n";
				} else if (command == 5) {
					stream.Close();
				}
			} catch (const std::exception &error) {
				std::cout << "Ошибка потока: " << error.what() << "\n";
			}
		}
	}

	void StringReadPanel() {
		std::string text = ReadLine("Введите целые числа через пробел: ");
		StringReadStream<Number> stream(text, [](const std::string &token) { return std::stoll(token); });
		ReadStreamCommands(stream);
	}

	void SequenceWritePanel() {
		MutableArraySequence<Number> destination;
		SequenceWriteStream<Number> stream(destination);
		while (true) {
			std::cout << "\nПоток записи в последовательность\n";
			std::cout << "1. Open\n2. Write\n3. Показать результат и позицию\n4. Close\n0. Назад\n";
			int choice = ReadValue<int>("Выбор: ");
			if (choice == 0) {
				return;
			}
			try {
				if (choice == 1) {
					stream.Open();
				} else if (choice == 2) {
					std::cout << "Новая позиция: " << stream.Write(ReadValue<Number>("Значение: ")) << "\n";
				} else if (choice == 3) {
					PrintFiniteSequence(destination);
					std::cout << "Позиция записи: " << stream.GetPosition() << "\n";
				} else if (choice == 4) {
					stream.Close();
				}
			} catch (const std::exception &error) {
				std::cout << "Ошибка потока: " << error.what() << "\n";
			}
		}
	}

	void FileStreamsPanel() {
		std::string filename = ReadLine("Имя файла: ");
		bool appendMode = ReadValue<int>("Запись в конец файла? (1 - да, 0 - нет): ") != 0;
		FileWriteStream<Number> writer(filename, [](const Number &value) { return std::to_string(value); }, appendMode);
		FileReadStream<Number> reader(filename, [](const std::string &line) { return std::stoll(line); });
		while (true) {
			std::cout << "\nФайловые потоки\n";
			std::cout << "1. Открыть запись\n2. Записать число\n3. Позиция записи\n4. Закрыть запись\n";
			std::cout << "5. Открыть чтение\n6. Прочитать число\n7. Seek чтения\n8. Состояние чтения\n9. Закрыть чтение\n";
			std::cout << "0. Назад\n";
			int choice = ReadValue<int>("Выбор: ");
			if (choice == 0) {
				return;
			}
			try {
				if (choice == 1) {
					writer.Open();
				} else if (choice == 2) {
					std::cout << "Новая позиция: " << writer.Write(ReadValue<Number>("Значение: ")) << "\n";
				} else if (choice == 3) {
					std::cout << "Позиция записи: " << writer.GetPosition() << "\n";
				} else if (choice == 4) {
					writer.Close();
				} else if (choice == 5) {
					reader.Open();
				} else if (choice == 6) {
					std::cout << "Прочитано: " << reader.Read() << "\n";
				} else if (choice == 7) {
					std::cout << "Новая позиция: " << reader.Seek(ReadValue<std::size_t>("Позиция: ")) << "\n";
				} else if (choice == 8) {
					PrintReadStreamStatus(reader);
				} else if (choice == 9) {
					reader.Close();
				}
			} catch (const std::exception &error) {
				std::cout << "Ошибка файлового потока: " << error.what() << "\n";
			}
		}
	}

	struct ForecastSettings {
		std::size_t order = 1;
		std::size_t historySize = 8;
		double warningThreshold = 10.0;
		double criticalThreshold = 25.0;
	};

	EventType ReadEventType() {
		std::cout << "1. CPU_LOAD\n2. MEMORY_LOAD\n3. TEMPERATURE\n4. REQUEST_ERRORS\n";
		int choice = ReadValue<int>("Тип события: ");
		if (choice == 1) {
			return EventType::CpuLoad;
		}
		if (choice == 2) {
			return EventType::MemoryLoad;
		}
		if (choice == 3) {
			return EventType::Temperature;
		}
		if (choice == 4) {
			return EventType::RequestErrors;
		}
		throw std::invalid_argument("Неизвестный тип события");
	}

	void ConfigureForecast(ForecastSettings &settings) {
		settings.order = ReadValue<std::size_t>("Порядок прогноза (1 или 2): ");
		settings.historySize = ReadValue<std::size_t>("Максимальный размер истории: ");
		settings.warningThreshold = ReadValue<double>("Порог WARNING: ");
		settings.criticalThreshold = ReadValue<double>("Порог CRITICAL: ");
		ForecastCorrectionProcessor validation(settings.order, settings.historySize,
		                                       settings.warningThreshold, settings.criticalThreshold);
		std::cout << "Настройки приняты.\n";
	}

	void PrintForecastSettings(const ForecastSettings &settings) {
		std::cout << "Порядок: " << settings.order
		          << ", история: " << settings.historySize
		          << ", WARNING: " << settings.warningThreshold
		          << ", CRITICAL: " << settings.criticalThreshold << "\n";
	}

	void PrintProcessingResult(const ProcessingResult &result) {
		std::cout << "\nСобытие #" << result.currentEvent.id
		          << ", t=" << result.currentEvent.timestamp
		          << ", " << EventTypeToString(result.currentEvent.type)
		          << "=" << result.currentEvent.value
		          << ", источник=" << result.currentEvent.source << "\n";
		std::cout << "Предыдущий прогноз: ";
		if (result.previousPrediction.hasPrediction) {
			std::cout << result.previousPrediction.predictedValue
			          << " (confidence=" << result.previousPrediction.confidence << ")\n";
		} else {
			std::cout << "нет\n";
		}
		std::cout << "Коррекция: ";
		if (result.correction.hasCorrection) {
			std::cout << "ошибка=" << result.correction.error
			          << ", абсолютная ошибка=" << result.correction.absoluteError << "\n";
		} else {
			std::cout << "не выполнялась\n";
		}
		std::cout << "Реакция: " << ReactionTypeToString(result.reaction.type)
		          << " - " << result.reaction.message << "\n";
		std::cout << "Следующий прогноз: ";
		if (result.nextPrediction.hasPrediction) {
			std::cout << result.nextPrediction.predictedValue << "\n";
		} else {
			std::cout << "ещё нет данных\n";
		}
	}

	void PrintStatistics(const ProcessingStatistics &statistics, std::size_t materializedCount,
	                     long long elapsedMilliseconds) {
		std::cout << "\nСтатистика обработки\n";
		std::cout << "Событий: " << statistics.GetTotal() << "\n";
		std::cout << "NO_PREDICTION: " << statistics.GetNoPredictionCount() << "\n";
		std::cout << "NORMAL: " << statistics.GetNormalCount() << "\n";
		std::cout << "WARNING: " << statistics.GetWarningCount() << "\n";
		std::cout << "CRITICAL: " << statistics.GetCriticalCount() << "\n";
		std::cout << "Средняя абсолютная ошибка: " << statistics.GetMeanAbsoluteError() << "\n";
		std::cout << "Максимальная абсолютная ошибка: " << statistics.GetMaximumAbsoluteError() << "\n";
		std::cout << "Материализовано событий LazySequence: " << materializedCount << "\n";
		std::cout << "Время: " << elapsedMilliseconds << " мс\n";
	}

	void SaveProcessingLog(const MutableArraySequence<ProcessingResult> &log) {
		std::string filename = ReadLine("Имя CSV-файла для сохранения лога: ");
		FileWriteStream<ProcessingResult> writer(filename, SerializeProcessingResult);
		writer.Open();
		std::unique_ptr<IEnumerator<ProcessingResult> > enumerator(log.GetEnumerator());
		while (enumerator->MoveNext()) {
			writer.Write(enumerator->Current());
		}
		writer.Close();
		std::cout << "Лог сохранён в " << filename << "\n";
	}

	void ProcessEventScenario(LazySequence<Event> &events, const ForecastSettings &settings,
	                          bool stepByStep, std::size_t maximumCount) {
		ForecastCorrectionProcessor processor(settings.order, settings.historySize,
		                                      settings.warningThreshold, settings.criticalThreshold);
		ProcessingStatistics statistics;
		MutableArraySequence<ProcessingResult> log;
		SequenceWriteStream<ProcessingResult> output(log);
		LazySequenceReadStream<Event> input(events);
		input.Open();
		output.Open();

		auto start = std::chrono::steady_clock::now();
		std::size_t processed = 0;
		while (processed < maximumCount && !input.IsEndOfStream()) {
			ProcessingResult result = processor.Process(input.Read());
			output.Write(result);
			statistics.Add(result);
			++processed;
			if (stepByStep) {
				PrintProcessingResult(result);
				if (ReadValue<int>("Продолжить? (1 - да, 0 - остановить): ") == 0) {
					break;
				}
			}
		}
		auto finish = std::chrono::steady_clock::now();
		input.Close();
		output.Close();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

		PrintStatistics(statistics, events.GetMaterializedCount(), elapsed);
		if (ReadValue<int>("Сохранить лог результатов? (1 - да, 0 - нет): ") == 1) {
			SaveProcessingLog(log);
		}
	}

	std::unique_ptr<LazySequence<Event> > ReadManualEvents() {
		auto count = ReadValue<std::size_t>("Количество событий: ");
		MutableArraySequence<Event> events;
		for (std::size_t i = 0; i < count; ++i) {
			std::cout << "\nСобытие #" << (i + 1) << "\n";
			EventType type = ReadEventType();
			auto timestamp = ReadValue<long long>("Время: ");
			auto value = ReadValue<double>("Значение: ");
			std::string source = ReadLine("Источник: ");
			events.Append(Event{i + 1, timestamp, type, value, source});
		}
		return std::make_unique<LazySequence<Event> >(static_cast<const Sequence<Event> &>(events));
	}

	std::unique_ptr<LazySequence<Event> > ReadEventsFromFile() {
		std::string filename = ReadLine("CSV-файл событий: ");
		FileReadStream<Event> reader(filename, DeserializeEvent);
		MutableArraySequence<Event> events;
		reader.Open();
		while (true) {
			try {
				events.Append(reader.Read());
			} catch (const EndOfStream &) {
				break;
			}
		}
		reader.Close();
		return std::make_unique<LazySequence<Event> >(static_cast<const Sequence<Event> &>(events));
	}

	void RunForecastScenario(const std::unique_ptr<LazySequence<Event> > &events, const ForecastSettings &settings) {
		std::size_t available = events->GetLength().IsFinite()
			                        ? events->GetLength().FinitePart()
			                        : ReadValue<std::size_t>("Количество обрабатываемых событий: ");
		auto count = ReadValue<std::size_t>("Сколько событий обработать (не более доступных): ");
		if (count > available) {
			count = available;
		}
		bool stepByStep = ReadValue<int>("Пошаговый вывод? (1 - да, 0 - нет): ") == 1;
		ProcessEventScenario(*events, settings, stepByStep, count);
	}

	void ForecastCorrectionPanel() {
		ForecastSettings settings;
		while (true) {
			std::cout << "\nВариант 3.3: прогноз / коррекция\n";
			PrintForecastSettings(settings);
			std::cout << "1. Настройки модели\n";
			std::cout << "2. Ручной ввод событий\n";
			std::cout << "3. Линейный поток (точный прогноз)\n";
			std::cout << "4. Поток с резким скачком\n";
			std::cout << "5. Поток с шумом\n";
			std::cout << "6. Загрузить события из CSV-файла\n";
			std::cout << "7. Автоматический большой поток со скачком\n";
			std::cout << "0. Назад\n";
			int choice = ReadValue<int>("Выбор: ");
			if (choice == 0) {
				return;
			}
			try {
				if (choice == 1) {
					ConfigureForecast(settings);
				} else if (choice == 2) {
					RunForecastScenario(ReadManualEvents(), settings);
				} else if (choice == 3) {
					EventType type = ReadEventType();
					auto count = ReadValue<std::size_t>("Количество событий: ");
					auto first = ReadValue<double>("Первое значение: ");
					auto step = ReadValue<double>("Шаг: ");
					RunForecastScenario(EventGenerator::Linear(type, count, first, step, "linear"), settings);
				} else if (choice == 4) {
					EventType type = ReadEventType();
					auto count = ReadValue<std::size_t>("Количество событий: ");
					auto first = ReadValue<double>("Первое значение: ");
					auto step = ReadValue<double>("Шаг до скачка: ");
					auto spike = ReadValue<std::size_t>("Индекс скачка (с нуля): ");
					auto spikeValue = ReadValue<double>("Значение скачка: ");
					RunForecastScenario(EventGenerator::WithSpike(type, count, first, step, spike,
					                                             spikeValue, "spike"), settings);
				} else if (choice == 5) {
					EventType type = ReadEventType();
					auto count = ReadValue<std::size_t>("Количество событий: ");
					auto base = ReadValue<double>("Базовое значение: ");
					auto amplitude = ReadValue<double>("Амплитуда шума: ");
					RunForecastScenario(EventGenerator::Noise(type, count, base, amplitude, "noise"), settings);
				} else if (choice == 6) {
					RunForecastScenario(ReadEventsFromFile(), settings);
				} else if (choice == 7) {
					auto count = ReadValue<std::size_t>("Количество событий: ");
					std::size_t spike = count / 2;
					ProcessEventScenario(*EventGenerator::WithSpike(EventType::CpuLoad, count, 20.0, 0.01,
					                                                spike, 95.0, "large"),
					                     settings, false, count);
				} else {
					std::cout << "Неизвестная команда.\n";
				}
			} catch (const std::exception &error) {
				std::cout << "Ошибка сценария: " << error.what() << "\n";
			}
		}
	}

	void RunLazyLoadTest() {
		auto count = ReadValue<std::size_t>("Сколько натуральных чисел обработать: ");
		auto sequence = Lazy::Infinite([](std::size_t index) { return static_cast<Number>(index + 1); });
		LazySequenceReadStream<Number> stream(*sequence);
		stream.Open();
		Number sum = 0;
		auto start = std::chrono::steady_clock::now();
		for (std::size_t i = 0; i < count; ++i) {
			sum += stream.Read();
		}
		auto finish = std::chrono::steady_clock::now();
		stream.Close();
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		std::cout << "Сумма: " << sum << "\n";
		std::cout << "Материализовано: " << sequence->GetMaterializedCount() << "\n";
		std::cout << "Время: " << milliseconds << " мс\n";
	}

	void RunFileLoadTest() {
		auto count = ReadValue<std::size_t>("Сколько чисел записать и прочитать: ");
		const std::string filename = "ui_file_stream_load_test.tmp";
		FileWriteStream<Number> writer(filename, [](const Number &value) { return std::to_string(value); });
		auto start = std::chrono::steady_clock::now();
		writer.Open();
		for (std::size_t i = 0; i < count; ++i) {
			writer.Write(static_cast<Number>(i + 1));
		}
		writer.Close();
		FileReadStream<Number> reader(filename, [](const std::string &value) { return std::stoll(value); });
		reader.Open();
		Number sum = 0;
		while (true) {
			try {
				sum += reader.Read();
			} catch (const EndOfStream &) {
				break;
			}
		}
		reader.Close();
		auto finish = std::chrono::steady_clock::now();
		std::remove(filename.c_str());
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		std::cout << "Сумма прочитанных значений: " << sum << "\n";
		std::cout << "Время записи и чтения: " << milliseconds << " мс\n";
	}

	void PrintMainMenu() {
		std::cout << "\nИнтерфейс лабораторной работы N4\n";
		std::cout << "1. Конечная LazySequence\n";
		std::cout << "2. LazySequence через генератор\n";
		std::cout << "3. Поток чтения обычной последовательности\n";
		std::cout << "4. Поток чтения LazySequence\n";
		std::cout << "5. Поток чтения строки\n";
		std::cout << "6. Поток записи в последовательность\n";
		std::cout << "7. Файловые потоки\n";
		std::cout << "9. Нагрузочная проверка LazySequence\n";
		std::cout << "10. Нагрузочная проверка файловых потоков\n";
		std::cout << "11. Вариант 3.3: прогноз / коррекция событий\n";
		std::cout << "0. Выход\n";
	}
}

void RunTestingUi() {
	while (true) {
		PrintMainMenu();
		int choice = ReadValue<int>("Выбор: ");
		try {
			if (choice == 1) {
				LazyWorkspace(CreateFiniteLazy());
			} else if (choice == 2) {
				LazyWorkspace(CreateGeneratedLazy());
			} else if (choice == 3) {
				SequenceReadPanel();
			} else if (choice == 4) {
				LazyReadPanel();
			} else if (choice == 5) {
				StringReadPanel();
			} else if (choice == 6) {
				SequenceWritePanel();
			} else if (choice == 7) {
				FileStreamsPanel();
			}else if (choice == 9) {
				RunLazyLoadTest();
			} else if (choice == 10) {
				RunFileLoadTest();
			} else if (choice == 11) {
				ForecastCorrectionPanel();
			} else if (choice == 0) {
				return;
			} else {
				std::cout << "Неизвестный пункт меню.\n";
			}
		} catch (const std::exception &error) {
			std::cout << "Ошибка: " << error.what() << "\n";
		}
	}
}
