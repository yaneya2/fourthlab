#ifndef FOURTHLAB_STREAMS_H
#define FOURTHLAB_STREAMS_H

#include <cstddef>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "IEnumerator.h"
#include "LazySequence.h"
#include "Sequence.h"

class EndOfStream : public std::out_of_range {
public:
	explicit EndOfStream(const std::string &message) : std::out_of_range(message) {
	}
};

class StreamException : public std::runtime_error {
public:
	explicit StreamException(const std::string &message) : std::runtime_error(message) {
	}
};

template<class T>
class ReadOnlyStream {
public:
	virtual ~ReadOnlyStream() = default;

	virtual void Open() = 0;

	virtual void Close() = 0;

	[[nodiscard]] virtual bool IsEndOfStream() const = 0;

	virtual T Read() = 0;

	[[nodiscard]] virtual std::size_t GetPosition() const = 0;

	[[nodiscard]] virtual bool IsCanSeek() const = 0;

	virtual std::size_t Seek(std::size_t index) = 0;

	[[nodiscard]] virtual bool IsCanGoBack() const = 0;
};

template<class T>
class WriteOnlyStream {
public:
	virtual ~WriteOnlyStream() = default;

	virtual void Open() = 0;

	virtual void Close() = 0;

	virtual std::size_t Write(const T &item) = 0;

	[[nodiscard]] virtual std::size_t GetPosition() const = 0;
};

template<class T>
class SequenceReadStream : public ReadOnlyStream<T> {
private:
	std::shared_ptr<Sequence<T> > source;
	std::unique_ptr<IEnumerator<T> > enumerator;
	std::size_t position;
	bool opened;

public:
	explicit SequenceReadStream(Sequence<T> &sourceSequence)
		: source(std::shared_ptr<Sequence<T> >(&sourceSequence, [](auto) {
		  })),
		  enumerator(nullptr), position(0), opened(false) {
	}

	explicit SequenceReadStream(std::shared_ptr<Sequence<T> > sourceSequence)
		: source(std::move(sourceSequence)),
		  enumerator(nullptr), position(0), opened(false) {
		if (!source) {
			throw std::invalid_argument("SequenceReadStream source is null");
		}
	}

	void Open() override {
		enumerator = std::unique_ptr<IEnumerator<T> >(source->GetEnumerator());
		position = 0;
		opened = true;
	}

	void Close() override {
		enumerator.reset();
		position = 0;
		opened = false;
	}

	[[nodiscard]] bool IsEndOfStream() const override {
		return position >= source->GetLength();
	}

	T Read() override {
		EnsureOpened();
		if (IsEndOfStream() || !enumerator->MoveNext()) {
			throw EndOfStream("SequenceReadStream reached end");
		}
		++position;
		return enumerator->Current();
	}

	[[nodiscard]] std::size_t GetPosition() const override {
		return position;
	}

	[[nodiscard]] bool IsCanSeek() const override {
		return true;
	}

	std::size_t Seek(std::size_t index) override {
		EnsureOpened();
		if (index > source->GetLength()) {
			throw EndOfStream("Seek index is out of range");
		}

		enumerator = std::unique_ptr<IEnumerator<T> >(source->GetEnumerator());
		position = 0;
		while (position < index) {
			if (!enumerator->MoveNext()) {
				throw EndOfStream("Seek index is out of range");
			}
			++position;
		}
		return position;
	}

	[[nodiscard]] bool IsCanGoBack() const override {
		return true;
	}

private:
	void EnsureOpened() const {
		if (!opened) {
			throw StreamException("Stream is not opened");
		}
	}
};

template<class T>
class LazySequenceReadStream : public ReadOnlyStream<T> {
private:
	std::shared_ptr<LazySequence<T> > source;
	std::size_t position;
	bool opened;

public:
	explicit LazySequenceReadStream(LazySequence<T> &sourceSequence)
		: source(std::shared_ptr<LazySequence<T> >(&sourceSequence, [](auto) {
		  })),
		  position(0), opened(false) {
	}

	explicit LazySequenceReadStream(std::shared_ptr<LazySequence<T> > sourceSequence)
		: source(std::move(sourceSequence)),
		  position(0), opened(false) {
		if (!source) {
			throw std::invalid_argument("LazySequenceReadStream source is null");
		}
	}

	void Open() override {
		position = 0;
		opened = true;
	}

	void Close() override {
		position = 0;
		opened = false;
	}

	[[nodiscard]] bool IsEndOfStream() const override {
		Ordinal length = source->GetLength();
		return length.IsFinite() && position >= length.FinitePart();
	}

	T Read() override {
		EnsureOpened();
		if (IsEndOfStream()) {
			throw EndOfStream("LazySequenceReadStream reached end");
		}
		T value = source->Get(Ordinal::Finite(position));
		++position;
		return value;
	}

	[[nodiscard]] std::size_t GetPosition() const override {
		return position;
	}

	[[nodiscard]] bool IsCanSeek() const override {
		return true;
	}

	std::size_t Seek(std::size_t index) override {
		EnsureOpened();
		Ordinal length = source->GetLength();
		if (length.IsFinite() && index > length.FinitePart()) {
			throw EndOfStream("Seek index is out of range");
		}
		position = index;
		return position;
	}

	[[nodiscard]] bool IsCanGoBack() const override {
		return true;
	}

private:
	void EnsureOpened() const {
		if (!opened) {
			throw StreamException("Stream is not opened");
		}
	}
};

template<class T>
class StringReadStream : public ReadOnlyStream<T> {
private:
	std::string source;
	std::function<T(const std::string &)> deserializer;
	std::istringstream input;
	std::size_t position;
	bool opened;
	bool endReached;

public:
	StringReadStream(std::string text, std::function<T(const std::string &)> itemDeserializer)
		: source(std::move(text)), deserializer(std::move(itemDeserializer)), input(), position(0),
		  opened(false), endReached(false) {
		if (!deserializer) {
			throw std::invalid_argument("Deserializer is empty");
		}
	}

	void Open() override {
		input.clear();
		input.str(source);
		position = 0;
		opened = true;
		endReached = false;
	}

	void Close() override {
		input.clear();
		input.str("");
		position = 0;
		opened = false;
		endReached = false;
	}

	bool IsEndOfStream() const override {
		return endReached;
	}

	T Read() override {
		EnsureOpened();
		std::string token;
		if (!(input >> token)) {
			endReached = true;
			throw EndOfStream("StringReadStream reached end");
		}
		++position;
		return deserializer(token);
	}

	std::size_t GetPosition() const override {
		return position;
	}

	bool IsCanSeek() const override {
		return true;
	}

	std::size_t Seek(std::size_t index) override {
		EnsureOpened();
		input.clear();
		input.str(source);
		position = 0;
		endReached = false;

		std::string ignored;
		while (position < index) {
			if (!(input >> ignored)) {
				endReached = true;
				throw EndOfStream("Seek index is out of string range");
			}
			++position;
		}
		return position;
	}

	bool IsCanGoBack() const override {
		return true;
	}

private:
	void EnsureOpened() const {
		if (!opened) {
			throw StreamException("Stream is not opened");
		}
	}
};

template<class T>
class FileReadStream : public ReadOnlyStream<T> {
private:
	std::string filename;
	std::function<T(const std::string &)> deserializer;
	std::ifstream input;
	std::size_t position;
	bool opened;
	bool endReached;

public:
	FileReadStream(std::string fileName, std::function<T(const std::string &)> itemDeserializer)
		: filename(std::move(fileName)), deserializer(std::move(itemDeserializer)), input(), position(0),
		  opened(false), endReached(false) {
		if (!deserializer) {
			throw std::invalid_argument("Deserializer is empty");
		}
	}

	void Open() override {
		Close();
		input.open(filename);
		if (!input.is_open()) {
			throw StreamException("Cannot open file for reading: " + filename);
		}
		position = 0;
		opened = true;
		endReached = false;
	}

	void Close() override {
		if (input.is_open()) {
			input.close();
		}
		position = 0;
		opened = false;
		endReached = false;
	}

	bool IsEndOfStream() const override {
		return endReached;
	}

	T Read() override {
		EnsureOpened();
		std::string line;
		if (!std::getline(input, line)) {
			endReached = true;
			throw EndOfStream("FileReadStream reached end");
		}
		++position;
		return deserializer(line);
	}

	std::size_t GetPosition() const override {
		return position;
	}

	bool IsCanSeek() const override {
		return true;
	}

	std::size_t Seek(std::size_t index) override {
		EnsureOpened();
		input.clear();
		input.seekg(0, std::ios::beg);
		if (!input) {
			throw StreamException("Cannot seek file: " + filename);
		}

		position = 0;
		endReached = false;
		std::string ignored;
		while (position < index) {
			if (!std::getline(input, ignored)) {
				endReached = true;
				throw EndOfStream("Seek index is out of file range");
			}
			++position;
		}
		return position;
	}

	bool IsCanGoBack() const override {
		return true;
	}

private:
	void EnsureOpened() const {
		if (!opened) {
			throw StreamException("Stream is not opened");
		}
	}
};

template<class T>
class SequenceWriteStream : public WriteOnlyStream<T> {
private:
	std::shared_ptr<Sequence<T> > destination;
	std::size_t position;
	bool opened;

public:
	explicit SequenceWriteStream(Sequence<T> &destinationSequence)
		: destination(std::shared_ptr<Sequence<T> >(&destinationSequence, [](auto) {
		  })),
		  position(0), opened(false) {
	}

	explicit SequenceWriteStream(std::shared_ptr<Sequence<T> > destinationSequence)
		: destination(std::move(destinationSequence)),
		  position(0), opened(false) {
		if (!destination) {
			throw std::invalid_argument("SequenceWriteStream destination is null");
		}
	}

	void Open() override {
		position = 0;
		opened = true;
	}

	void Close() override {
		position = 0;
		opened = false;
	}

	std::size_t Write(const T &item) override {
		EnsureOpened();
		Sequence<T> *result = destination->Append(item);
		if (result == nullptr) {
			throw StreamException("SequenceWriteStream append returned null");
		}
		if (result != destination.get()) {
			destination.reset(result);
		}
		++position;
		return position;
	}

	[[nodiscard]] std::size_t GetPosition() const override {
		return position;
	}

private:
	void EnsureOpened() const {
		if (!opened) {
			throw StreamException("Stream is not opened");
		}
	}
};

template<class T>
class FileWriteStream : public WriteOnlyStream<T> {
private:
	std::string filename;
	std::function<std::string(const T &)> serializer;
	std::ofstream output;
	std::size_t position;
	bool opened;
	bool appendMode;

public:
	FileWriteStream(std::string fileName, std::function<std::string(const T &)> itemSerializer,
	                bool useAppendMode = false)
		: filename(std::move(fileName)), serializer(std::move(itemSerializer)), output(), position(0),
		  opened(false), appendMode(useAppendMode) {
		if (!serializer) {
			throw std::invalid_argument("Serializer is empty");
		}
	}

	void Open() override {
		Close();
		std::ios::openmode mode = std::ios::out;
		if (appendMode) {
			mode |= std::ios::app;
		}
		output.open(filename, mode);
		if (!output.is_open()) {
			throw StreamException("Cannot open file for writing: " + filename);
		}
		position = 0;
		opened = true;
	}

	void Close() override {
		if (output.is_open()) {
			output.close();
		}
		position = 0;
		opened = false;
	}

	std::size_t Write(const T &item) override {
		EnsureOpened();
		output << serializer(item) << '\n';
		if (!output) {
			throw StreamException("Cannot write to file: " + filename);
		}
		++position;
		return position;
	}

	std::size_t GetPosition() const override {
		return position;
	}

private:
	void EnsureOpened() const {
		if (!opened) {
			throw StreamException("Stream is not opened");
		}
	}
};

#endif
