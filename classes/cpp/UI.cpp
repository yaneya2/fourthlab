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

	std::string FormatLength(Cardinal length) {
		return length.IsOmega() ? "омега (бесконечная)" : std::to_string(length.Value());
	}

	Cardinal ReadLength() {
		std::cout << "1. Конечная длина\n";
		std::cout << "2. Бесконечная длина (омега)\n";
		int choice = ReadValue<int>("Выбор: ");
		if (choice == 2) {
			return Cardinal::Omega();
		}
		return Cardinal::Finite(ReadValue<std::size_t>("Длина: "));
	}

	MutableArraySequence<Number> ReadItems() {
		MutableArraySequence<Number> result;
		std::size_t count = ReadValue<std::size_t>("Количество элементов: ");
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
		Cardinal length = ReadLength();
		if (length.IsOmega()) {
			return Lazy::Infinite(std::move(rule));
		}
		return Lazy::FromIndexFunction(std::move(rule), length);
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
		int choice = ReadValue<int>("Выбор: ");

		if (choice == 1) {
			Number seedsData[] = {0, 1};
			MutableArraySequence<Number> seeds(seedsData, 2);
			return std::make_unique<Lazy>(FibonacciRule, &seeds, ReadLength());
		}
		if (choice == 2) {
			Number first = ReadValue<Number>("Первый элемент: ");
			Number step = ReadValue<Number>("Шаг: ");
			return CreateByIndexRule([first, step](std::size_t index) {
				return first + step * static_cast<Number>(index);
			});
		}
		if (choice == 3) {
			Number first = ReadValue<Number>("Первый элемент: ");
			Number ratio = ReadValue<Number>("Знаменатель прогрессии: ");
			MutableArraySequence<Number> seeds(&first, 1);
			auto rule = [ratio](Sequence<Number> *prefix) {
				std::size_t length = prefix->GetLength();
				return SequenceAt(prefix, length - 1) * ratio;
			};
			return std::make_unique<Lazy>(std::function<Number(Sequence<Number> *)>(rule), &seeds, ReadLength());
		}
		if (choice == 4) {
			return CreateByIndexRule([](std::size_t index) {
				Number n = static_cast<Number>(index + 1);
				return n * n;
			});
		}
		if (choice == 5) {
			return CreateByIndexRule([](std::size_t index) {
				Number n = static_cast<Number>(index + 1);
				return n * n * n;
			});
		}
		if (choice == 6) {
			Number first = 1;
			MutableArraySequence<Number> seeds(&first, 1);
			auto rule = [](Sequence<Number> *prefix) {
				std::size_t length = prefix->GetLength();
				return SequenceAt(prefix, length - 1) * static_cast<Number>(length + 1);
			};
			return std::make_unique<Lazy>(std::function<Number(Sequence<Number> *)>(rule), &seeds, ReadLength());
		}
		if (choice == 7) {
			Number a = ReadValue<Number>("a = ");
			Number b = ReadValue<Number>("b = ");
			return CreateByIndexRule([a, b](std::size_t index) {
				return a * static_cast<Number>(index) + b;
			});
		}
		if (choice == 8) {
			return CreateByIndexRule([](std::size_t index) {
				return static_cast<Number>(index + 1);
			});
		}
		throw std::invalid_argument("Неизвестный генератор");
	}

	std::size_t PrefixCount(const Lazy &sequence) {
		std::size_t requested = ReadValue<std::size_t>("Сколько элементов вывести: ");
		Cardinal length = sequence.GetLength();
		if (length.IsFinite() && requested > length.Value()) {
			return length.Value();
		}
		return requested;
	}

	void PrintLazyPrefix(const Lazy &sequence, std::size_t count) {
		std::cout << "Элементы: ";
		for (std::size_t i = 0; i < count; ++i) {
			if (i != 0) {
				std::cout << ' ';
			}
			std::cout << sequence.Get(i);
		}
		std::cout << "\nДлина: " << FormatLength(sequence.GetLength());
		std::cout << "\nМатериализовано: " << sequence.GetMaterializedCount() << "\n";
	}

	void PrintFiniteSequence(Sequence<Number> &sequence) {
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
			Number multiplier = ReadValue<Number>("Множитель: ");
			sequence = sequence->Map<Number>([multiplier](Number x) { return x * multiplier; });
		} else if (choice == 3) {
			Number a = ReadValue<Number>("a = ");
			Number b = ReadValue<Number>("b = ");
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
			Number bound = ReadValue<Number>("Граница: ");
			sequence = sequence->Where([bound](Number x) { return x > bound; });
		} else if (choice == 4) {
			Number divisor = ReadValue<Number>("Делитель: ");
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
		std::size_t count = ReadValue<std::size_t>("Сколько элементов перечислить: ");
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
			std::cout << "1. Вывести префикс\n2. Показать длину и размер кэша\n3. GetFirst / GetLast\n";
			std::cout << "4. Get по индексу\n5. Конечная подпоследовательность\n6. Бесконечный хвост\n";
			std::cout << "7. Append\n8. Prepend\n9. InsertAt элемента\n10. RemoveAt\n11. RemoveRange\n";
			std::cout << "12. Concat с введённой последовательностью\n13. Вставить введённую последовательность\n";
			std::cout << "14. Map\n15. Where\n16. Reduce всей последовательности\n17. ReduceFirstN\n";
			std::cout << "18. Take\n19. Обход через GetEnumerator\n0. Назад\n";
			int choice = ReadValue<int>("Выбор: ");
			if (choice == 0) {
				return;
			}
			try {
				if (choice == 1) {
					PrintLazyPrefix(*sequence, PrefixCount(*sequence));
				} else if (choice == 2) {
					std::cout << "Длина: " << FormatLength(sequence->GetLength()) << "\n";
					std::cout << "Материализовано: " << sequence->GetMaterializedCount() << "\n";
				} else if (choice == 3) {
					std::cout << "Первый элемент: " << sequence->GetFirst() << "\n";
					std::cout << "Последний элемент: " << sequence->GetLast() << "\n";
				} else if (choice == 4) {
					std::size_t index = ReadValue<std::size_t>("Индекс: ");
					std::cout << "Значение: " << sequence->Get(Cardinal::Finite(index)) << "\n";
				} else if (choice == 5) {
					std::size_t start = ReadValue<std::size_t>("Начальный индекс: ");
					std::size_t end = ReadValue<std::size_t>("Конечный индекс: ");
					sequence = sequence->GetSubsequence(Cardinal::Finite(start), Cardinal::Finite(end));
					std::cout << "Подпоследовательность стала текущей.\n";
				} else if (choice == 6) {
					std::size_t start = ReadValue<std::size_t>("Начальный индекс хвоста: ");
					sequence = sequence->GetSubsequence(Cardinal::Finite(start), Cardinal::Omega());
					std::cout << "Бесконечный хвост стал текущей последовательностью.\n";
				} else if (choice == 7) {
					sequence = sequence->Append(ReadValue<Number>("Значение: "));
				} else if (choice == 8) {
					sequence = sequence->Prepend(ReadValue<Number>("Значение: "));
				} else if (choice == 9) {
					Number item = ReadValue<Number>("Значение: ");
					std::size_t index = ReadValue<std::size_t>("Индекс: ");
					sequence = sequence->InsertAt(item, index);
				} else if (choice == 10) {
					sequence = sequence->RemoveAt(ReadValue<std::size_t>("Индекс: "));
				} else if (choice == 11) {
					std::size_t start = ReadValue<std::size_t>("Начальный индекс: ");
					std::size_t count = ReadValue<std::size_t>("Количество удаляемых элементов: ");
					sequence = sequence->RemoveRange(start, count);
				} else if (choice == 12) {
					auto second = CreateFiniteLazy();
					sequence = sequence->Concat(*second);
				} else if (choice == 13) {
					MutableArraySequence<Number> inserted = ReadItems();
					std::size_t index = ReadValue<std::size_t>("Индекс вставки: ");
					sequence = sequence->InsertAt(static_cast<const Sequence<Number> &>(inserted), index);
				} else if (choice == 14) {
					MapCurrent(sequence);
				} else if (choice == 15) {
					FilterCurrent(sequence);
				} else if (choice == 16) {
					ReduceCurrent(*sequence, false);
				} else if (choice == 17) {
					ReduceCurrent(*sequence, true);
				} else if (choice == 18) {
					auto result = sequence->Take(ReadValue<std::size_t>("Количество элементов: "));
					PrintFiniteSequence(*result);
				} else if (choice == 19) {
					EnumerateCurrent(*sequence);
				} else {
					std::cout << "Неизвестная команда.\n";
				}
			} catch (const std::exception &error) {
				std::cout << "Ошибка операции: " << error.what() << "\n";
			}
		}
	}

	void PrintReadStreamStatus(ReadOnlyStream<Number> &stream) {
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

	void RunAutomaticChecks() {
		int items[] = {1, 2, 3};
		LazySequence<int> sequence(items, 3);
		if (sequence.Get(1) != 2 || sequence.GetMaterializedCount() != 2) {
			throw std::runtime_error("Не пройдена проверка материализации");
		}
		auto changed = sequence.Prepend(0)->Append(4);
		if (changed->GetFirst() != 0 || changed->GetLast() != 4) {
			throw std::runtime_error("Не пройдена проверка операций");
		}
		StringReadStream<int> stream("7 8", DeserializeInt);
		stream.Open();
		if (stream.Read() != 7 || stream.Read() != 8) {
			throw std::runtime_error("Не пройдена проверка потоков");
		}
		std::cout << "Встроенные проверки UI успешно выполнены. Полный набор запускается через GoogleTest.\n";
	}

	void RunLazyLoadTest() {
		std::size_t count = ReadValue<std::size_t>("Сколько натуральных чисел обработать: ");
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
		std::size_t count = ReadValue<std::size_t>("Сколько чисел записать и прочитать: ");
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
		std::cout << "8. Встроенные автоматические проверки\n";
		std::cout << "9. Нагрузочная проверка LazySequence\n";
		std::cout << "10. Нагрузочная проверка файловых потоков\n";
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
			} else if (choice == 8) {
				RunAutomaticChecks();
			} else if (choice == 9) {
				RunLazyLoadTest();
			} else if (choice == 10) {
				RunFileLoadTest();
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
