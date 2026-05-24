#include "UI.h"

#include <chrono>
#include <clocale>
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
	class UiTestFailure : public std::runtime_error {
	public:
		explicit UiTestFailure(const std::string &message) : std::runtime_error(message) {
		}
	};

	void ClearInputLine() {
		std::cin.clear();
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}

	template<class T>
	T ReadValue(const std::string &prompt) {
		T value{};
		while (true) {
			std::cout << prompt;
			if (std::cin >> value) {
				return value;
			}
			ClearInputLine();
			std::cout << "Некорректный ввод. Попробуйте ещё раз.\n";
		}
	}

	std::string ReadTextLine(const std::string &prompt) {
		std::cout << prompt;
		std::string text;
		std::getline(std::cin >> std::ws, text);
		return text;
	}

	std::string FormatCardinal(const Cardinal &value) {
		if (value.IsOmega()) {
			return "омега (бесконечность)";
		}
		return std::to_string(value.Value());
	}

	void Require(bool condition, const std::string &message) {
		if (!condition) {
			throw UiTestFailure(message);
		}
	}

	template<class T>
	void RequireEqual(const T &actual, const T &expected, const std::string &message) {
		if (!(actual == expected)) {
			throw UiTestFailure(message);
		}
	}

	template<class ExceptionT, class Action>
	void RequireThrows(Action action, const std::string &message) {
		try {
			action();
		} catch (const ExceptionT &) {
			return;
		} catch (...) {
			throw UiTestFailure(message + ": выброшено исключение неверного типа");
		}
		throw UiTestFailure(message + ": исключение не было выброшено");
	}

	template <class T>
	T GetAt(Sequence<T> *sequence, std::size_t index) {
		auto *enumerator = sequence->GetEnumerator();
		for (std::size_t i = 0; i <= index; ++i) {
			if (!enumerator->MoveNext()) {
				delete enumerator;
				throw std::out_of_range("Индекс вне диапазона");
			}
		}
		T value = enumerator->Current();
		delete enumerator;
		return value;
	}

	long long FibonacciRule(Sequence<long long> *generated) {
		std::size_t length = generated->GetLength();
		if (length < 2) {
			throw std::logic_error("Для Фибоначчи нужны два начальных элемента");
		}
		return GetAt(generated, length - 1) + GetAt(generated, length - 2);
	}

	template <class T>
	void PrintSequencePrefix(LazySequence<T> &sequence, std::size_t count) {
		std::cout << "Значения: ";
		for (std::size_t i = 0; i < count; ++i) {
			if (i > 0) {
				std::cout << ' ';
			}
			std::cout << sequence.Get(i);
		}
		std::cout << "\nДлина: " << FormatCardinal(sequence.GetLength());
		std::cout << "\nМатериализовано элементов: " << sequence.GetMaterializedCount() << "\n";
	}

	void PrintFiniteSequence(Sequence<int> *sequence) {
		std::cout << "Значения: ";
		auto *enumerator = sequence->GetEnumerator();
		bool first = true;
		while (enumerator->MoveNext()) {
			if (!first) {
				std::cout << ' ';
			}
			std::cout << enumerator->Current();
			first = false;
		}
		delete enumerator;
		std::cout << "\nДлина: " << sequence->GetLength() << "\n";
	}

	void PrintGeneratorCacheCheck(LazySequence<long long> &sequence, std::size_t count) {
		std::cout << "\nПроверка кэша: повторное обращение к уже вычисленному префиксу не увеличивает кэш.\n";
		std::size_t before = sequence.GetMaterializedCount();
		if (count > 0) {
			sequence.Get(count - 1);
		}
		std::size_t after = sequence.GetMaterializedCount();
		std::cout << "До повторного обращения: " << before << "\n";
		std::cout << "После повторного обращения: " << after << "\n";
	}

	void ManualLazySequenceTest() {
		std::size_t count = ReadValue<std::size_t>("Количество элементов: ");
		MutableArraySequence<int> source;
		for (std::size_t i = 0; i < count; ++i) {
			source.Append(ReadValue<int>("элемент[" + std::to_string(i) + "] = "));
		}

		LazySequence<int> lazy(&source);
		std::cout << "\nСоздана конечная ленивая последовательность из данных, введённых с клавиатуры.\n";
		std::cout << "Материализовано до обращения: " << lazy.GetMaterializedCount() << "\n";
		PrintSequencePrefix(lazy, count);

		while (true) {
			std::cout << "\nРучные проверки ленивой последовательности\n";
			std::cout << "1. Получить элемент по индексу\n";
			std::cout << "2. Получить подпоследовательность\n";
			std::cout << "3. Предпросмотр добавления и вставки\n";
			std::cout << "0. Назад\n";
			std::cout << "> ";

			int choice = ReadValue<int>("");
			if (choice == 0) {
				return;
			}

			try {
				if (choice == 1) {
					std::size_t index = ReadValue<std::size_t>("индекс = ");
					std::cout << "значение = " << lazy.Get(index) << "\n";
					std::cout << "Материализовано сейчас: " << lazy.GetMaterializedCount() << "\n";
				} else if (choice == 2) {
					std::size_t start = ReadValue<std::size_t>("начальный индекс = ");
					std::size_t end = ReadValue<std::size_t>("конечный индекс = ");
					auto subsequence = lazy.GetSubsequence(start, end);
					PrintSequencePrefix(*subsequence, end - start + 1);
				} else if (choice == 3) {
					int value = ReadValue<int>("значение для операции = ");
					std::size_t index = ReadValue<std::size_t>("индекс вставки = ");
					auto changed = lazy.Prepend(value);
					auto appended = changed->Append(value);
					auto inserted = appended->InsertAt(value, index);
					std::size_t previewCount = inserted->GetLength().IsFinite() ? inserted->GetLength().Value() : 10;
					PrintSequencePrefix(*inserted, previewCount);
				} else {
					std::cout << "Неизвестный пункт меню\n";
				}
			} catch (const std::exception &error) {
				std::cout << "Проверка завершилась ошибкой: " << error.what() << "\n";
			}
		}
	}

	void ManualRecurrenceTest() {
		std::cout << "\nГенераторы ленивых последовательностей\n";
		std::cout << "1. Числа Фибоначчи\n";
		std::cout << "2. Арифметическая прогрессия\n";
		std::cout << "3. Геометрическая прогрессия\n";
		std::cout << "4. Квадраты натуральных чисел\n";
		std::cout << "5. Кубы натуральных чисел\n";
		std::cout << "6. Факториалы\n";
		std::cout << "7. Линейная формула a*n + b\n";
		std::cout << "0. Назад\n";
		std::cout << "> ";

		int generatorChoice = ReadValue<int>("");
		if (generatorChoice == 0) {
			return;
		}

		std::size_t count = ReadValue<std::size_t>("Сколько элементов вывести? ");

		if (generatorChoice == 1) {
			long long seedItems[] = {0, 1};
			MutableArraySequence<long long> seeds(seedItems, 2);
			LazySequence<long long> sequence(FibonacciRule, &seeds);
			PrintSequencePrefix(sequence, count);
			PrintGeneratorCacheCheck(sequence, count);
		} else if (generatorChoice == 2) {
			long long first = ReadValue<long long>("первый элемент = ");
			long long step = ReadValue<long long>("шаг = ");
			LazySequence<long long> sequence(
				std::function<long long(std::size_t)>(
					[first, step](std::size_t index) { return first + step * static_cast<long long>(index); }),
				Cardinal::Omega());
			PrintSequencePrefix(sequence, count);
			PrintGeneratorCacheCheck(sequence, count);
		} else if (generatorChoice == 3) {
			long long first = ReadValue<long long>("первый элемент = ");
			long long ratio = ReadValue<long long>("знаменатель прогрессии = ");
			MutableArraySequence<long long> seeds(&first, 1);
			LazySequence<long long> sequence(
				std::function<long long(Sequence<long long> *)>(
					[ratio](Sequence<long long> *generated) {
						std::size_t length = generated->GetLength();
						return GetAt(generated, length - 1) * ratio;
					}),
				&seeds);
			PrintSequencePrefix(sequence, count);
			PrintGeneratorCacheCheck(sequence, count);
		} else if (generatorChoice == 4) {
			LazySequence<long long> sequence(
				std::function<long long(std::size_t)>([](std::size_t index) {
					long long n = static_cast<long long>(index + 1);
					return n * n;
				}),
				Cardinal::Omega());
			PrintSequencePrefix(sequence, count);
			PrintGeneratorCacheCheck(sequence, count);
		} else if (generatorChoice == 5) {
			LazySequence<long long> sequence(
				std::function<long long(std::size_t)>([](std::size_t index) {
					long long n = static_cast<long long>(index + 1);
					return n * n * n;
				}),
				Cardinal::Omega());
			PrintSequencePrefix(sequence, count);
			PrintGeneratorCacheCheck(sequence, count);
		} else if (generatorChoice == 6) {
			long long first = 1;
			MutableArraySequence<long long> seeds(&first, 1);
			LazySequence<long long> sequence(
				std::function<long long(Sequence<long long> *)>([](Sequence<long long> *generated) {
					std::size_t length = generated->GetLength();
					return GetAt(generated, length - 1) * static_cast<long long>(length + 1);
				}),
				&seeds);
			PrintSequencePrefix(sequence, count);
			PrintGeneratorCacheCheck(sequence, count);
		} else if (generatorChoice == 7) {
			long long a = ReadValue<long long>("a = ");
			long long b = ReadValue<long long>("b = ");
			LazySequence<long long> sequence(
				std::function<long long(std::size_t)>(
					[a, b](std::size_t index) { return a * static_cast<long long>(index) + b; }),
				Cardinal::Omega());
			PrintSequencePrefix(sequence, count);
			PrintGeneratorCacheCheck(sequence, count);
		} else {
			std::cout << "Неизвестный пункт меню\n";
		}
	}

	void ManualStreamTest() {
		std::string text = ReadTextLine("Введите целые числа через пробел: ");
		StringReadStream<int> stream(text, DeserializeInt);
		stream.Open();

		std::cout << "Чтение потока:\n";
		long long sum = 0;
		while (true) {
			try {
				int value = stream.Read();
				sum += value;
				std::cout << "позиция=" << stream.GetPosition() << ", значение=" << value << "\n";
			} catch (const EndOfStream &) {
				break;
			}
		}

		std::cout << "Достигнут конец потока. Сумма = " << sum << "\n";
		stream.Close();
	}

	void SequenceWriteStreamTest() {
		std::size_t count = ReadValue<std::size_t>("Сколько значений записать в поток последовательности? ");
		MutableArraySequence<int> destination;
		SequenceWriteStream<int> stream(destination);
		stream.Open();

		for (std::size_t i = 0; i < count; ++i) {
			int value = ReadValue<int>("значение[" + std::to_string(i) + "] = ");
			std::cout << "следующая позиция = " << stream.Write(value) << "\n";
		}
		stream.Close();

		std::cout << "Последовательность после записи:\n";
		PrintFiniteSequence(&destination);
	}

	void RunNamedUiTest(const std::string &name, void (*test)(), int &passed, int &failed) {
		try {
			test();
			++passed;
			std::cout << "[ОК] " << name << "\n";
		} catch (const std::exception &error) {
			++failed;
			std::cout << "[ОШИБКА] " << name << ": " << error.what() << "\n";
		}
	}

	void AutoTestLazyMaterialization() {
		int items[] = {10, 20, 30};
		LazySequence<int> lazy(items, 3);
		RequireEqual(lazy.GetMaterializedCount(), static_cast<std::size_t>(0),
		             "кэш должен быть пустым до первого обращения");
		RequireEqual(lazy.Get(1), 20, "Get(1)");
		RequireEqual(lazy.GetMaterializedCount(), static_cast<std::size_t>(2),
		             "после Get(1) в кэше должны быть первые два элемента");
		RequireEqual(lazy.GetLast(), 30, "GetLast()");
               RequireThrows<std::out_of_range>([&lazy]() { lazy.Get(3); }, "выход за границы Get");
	}

	void AutoTestLazyOperations() {
		int items[] = {2, 3};
		LazySequence<int> base(items, 2);

		auto changed = base.Prepend(1);
		auto appended = changed->Append(4);
		auto inserted = appended->InsertAt(99, 2);
		auto removed = inserted->RemoveAt(2);

		RequireEqual(removed->GetLength(), Cardinal::Finite(4), "длина после операций");
		RequireEqual(removed->Get(0), 1, "первый элемент");
		RequireEqual(removed->Get(1), 2, "второй элемент");
		RequireEqual(removed->Get(2), 3, "третий элемент");
		RequireEqual(removed->Get(3), 4, "четвёртый элемент");
	}

	void AutoTestMapWhereReduce() {
		LazySequence<int> finite([](std::size_t index) { return static_cast<int>(index + 1); }, Cardinal::Finite(10));

		auto squares = finite.Map<int>(std::function<int(int)>([](int value) { return value * value; }));
		RequireEqual(squares->Get(4), 25, "отображение: квадрат");

		auto evens = finite.Where(std::function<bool(int)>([](int value) { return value % 2 == 0; }));
		RequireEqual(evens->GetLength(), Cardinal::Finite(5), "фильтрация: длина");
		RequireEqual(evens->Get(0), 2, "фильтрация: первый элемент");
		RequireEqual(evens->Get(4), 10, "фильтрация: последний элемент");

		int sum = finite.Reduce<int>(0, std::function<int(int, int)>([](int acc, int value) { return acc + value; }));
		RequireEqual(sum, 55, "свёртка: сумма");
	}

	void AutoTestStreams() {
		MutableArraySequence<int> sequence;
		sequence.Append(7)->Append(8)->Append(9);

		SequenceReadStream<int> sequenceStream(sequence);
		RequireThrows<StreamException>([&sequenceStream]() { sequenceStream.Read(); }, "чтение до Open");
		sequenceStream.Open();
		RequireEqual(sequenceStream.Read(), 7, "первое значение SequenceReadStream");
		RequireEqual(sequenceStream.Seek(2), static_cast<std::size_t>(2), "Seek в SequenceReadStream");
		RequireEqual(sequenceStream.Read(), 9, "чтение после Seek");
		sequenceStream.Close();

		StringReadStream<int> stringStream("1 2 3 4", DeserializeInt);
		stringStream.Open();
		RequireEqual(stringStream.Read(), 1, "первое значение StringReadStream");
		RequireEqual(stringStream.Seek(3), static_cast<std::size_t>(3), "Seek в StringReadStream");
		RequireEqual(stringStream.Read(), 4, "чтение после Seek в StringReadStream");
		stringStream.Close();
	}

	void AutoTestFileStreams() {
		const char *filename = "ui_stream_test.tmp";

		FileWriteStream<int> writer(filename, SerializeInt);
		writer.Open();
		writer.Write(21);
		writer.Write(34);
		writer.Close();

		FileReadStream<int> reader(filename, DeserializeInt);
		reader.Open();
		RequireEqual(reader.Read(), 21, "первое значение из файла");
		RequireEqual(reader.Read(), 34, "второе значение из файла");
		RequireThrows<EndOfStream>([&reader]() { reader.Read(); }, "конец файлового потока");
		reader.Close();

		std::remove(filename);
	}

	void RunAutomaticUiTests() {
		int passed = 0;
		int failed = 0;

		RunNamedUiTest("Материализация ленивой последовательности", AutoTestLazyMaterialization, passed, failed);
		RunNamedUiTest("Операции ленивой последовательности", AutoTestLazyOperations, passed, failed);
		RunNamedUiTest("Отображение, фильтрация, свёртка", AutoTestMapWhereReduce, passed, failed);
		RunNamedUiTest("Потоки", AutoTestStreams, passed, failed);
		RunNamedUiTest("Файловые потоки", AutoTestFileStreams, passed, failed);

		std::cout << "\nИтог автоматических проверок: успешно=" << passed << ", ошибок=" << failed << "\n";
	}

	void RunLoadTest() {
		std::size_t count = ReadValue<std::size_t>("Сколько элементов обработать? ");
		auto naturals = LazySequence<int>::Infinite([](std::size_t index) { return static_cast<int>(index + 1); });
		LazySequenceReadStream<int> stream(*naturals);
		stream.Open();

		long long sum = 0;
		auto start = std::chrono::steady_clock::now();
		for (std::size_t i = 0; i < count; ++i) {
			sum += stream.Read();
		}
		auto finish = std::chrono::steady_clock::now();
		stream.Close();

		auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
		std::cout << "Обработано элементов: " << count << "\n";
		std::cout << "Сумма: " << sum << "\n";
		std::cout << "Материализовано элементов ленивой последовательности: " << naturals->GetMaterializedCount()
				<< "\n";
		std::cout << "Время: " << elapsedMs << " мс\n";
	}

	void RunFileLoadTest() {
		std::size_t count = ReadValue<std::size_t>("Сколько чисел записать во временный файл? ");
		const std::string filename = "ui_file_stream_load_test.tmp";

		auto serializeLongLong = [](const long long &value) { return std::to_string(value); };
		auto deserializeLongLong = [](const std::string &text) { return std::stoll(text); };

		auto writeStart = std::chrono::steady_clock::now();
		FileWriteStream<long long> writer(filename, serializeLongLong);
		writer.Open();
		for (std::size_t i = 0; i < count; ++i) {
			writer.Write(static_cast<long long>(i + 1));
		}
		writer.Close();
		auto writeFinish = std::chrono::steady_clock::now();

		long long sum = 0;
		FileReadStream<long long> reader(filename, deserializeLongLong);
		auto readStart = std::chrono::steady_clock::now();
		reader.Open();
		while (true) {
			try {
				sum += reader.Read();
			} catch (const EndOfStream &) {
				break;
			}
		}
		reader.Close();
		auto readFinish = std::chrono::steady_clock::now();

		std::remove(filename.c_str());

		long long expected = static_cast<long long>(count) * static_cast<long long>(count + 1) / 2;
		auto writeMs = std::chrono::duration_cast<std::chrono::milliseconds>(writeFinish - writeStart).count();
		auto readMs = std::chrono::duration_cast<std::chrono::milliseconds>(readFinish - readStart).count();

		std::cout << "Записано чисел: " << count << "\n";
		std::cout << "Сумма при чтении: " << sum << "\n";
		std::cout << "Ожидаемая сумма: " << expected << "\n";
		std::cout << "Время записи: " << writeMs << " мс\n";
		std::cout << "Время чтения: " << readMs << " мс\n";
	}

	void PrintMainMenu() {
		std::cout << "\nИнтерфейс тестирования лабораторной работы №4\n";
		std::cout << "1. Ручная проверка ленивой последовательности\n";
		std::cout << "2. Ручная проверка генераторов ленивых последовательностей\n";
		std::cout << "3. Ручная проверка потока чтения из строки\n";
		std::cout << "4. Ручная проверка потока записи в последовательность\n";
		std::cout << "5. Запустить автоматические проверки\n";
		std::cout << "6. Нагрузочная проверка бесконечной ленивой последовательности через поток\n";
		std::cout << "7. Нагрузочная проверка файлового потока\n";
		std::cout << "0. Выход\n";
		std::cout << "> ";
	}
} // namespace

void RunTestingUi() {
	while (true) {
		PrintMainMenu();
		int choice = ReadValue<int>("");

		try {
			if (choice == 1) {
				ManualLazySequenceTest();
			} else if (choice == 2) {
				ManualRecurrenceTest();
			} else if (choice == 3) {
				ManualStreamTest();
			} else if (choice == 4) {
				SequenceWriteStreamTest();
			} else if (choice == 5) {
				RunAutomaticUiTests();
			} else if (choice == 6) {
				RunLoadTest();
			} else if (choice == 7) {
				RunFileLoadTest();
			} else if (choice == 0) {
				return;
			} else {
				std::cout << "Неизвестный пункт меню\n";
			}
		} catch (const std::exception &error) {
			std::cout << "Ошибка: " << error.what() << "\n";
		}
	}
}
