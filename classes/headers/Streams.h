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
    explicit EndOfStream(const std::string& message) : std::out_of_range(message) {}
};

class StreamException : public std::runtime_error {
public:
    explicit StreamException(const std::string& message) : std::runtime_error(message) {}
};

namespace StreamDetail {
    template<class T>
    std::shared_ptr<T> MakeNonOwningShared(T& object) {
        return std::shared_ptr<T>(&object, [](auto) {});
    }

    template<class T>
    std::shared_ptr<T> RequireShared(std::shared_ptr<T> object, const std::string& message) {
        if (!object) {
            throw std::invalid_argument(message);
        }
        return object;
    }
}

template <class T>
class ReadOnlyStream {
public:
    virtual ~ReadOnlyStream() = default;

    virtual void Open() = 0;
    virtual void Close() = 0;
    virtual bool IsEndOfStream() const = 0;
    virtual T Read() = 0;
    virtual std::size_t GetPosition() const = 0;
    virtual bool IsCanSeek() const = 0;
    virtual std::size_t Seek(std::size_t index) = 0;
    virtual bool IsCanGoBack() const = 0;
};

template <class T>
class WriteOnlyStream {
public:
    virtual ~WriteOnlyStream() = default;

    virtual void Open() = 0;
    virtual void Close() = 0;
    virtual std::size_t Write(const T& item) = 0;
    virtual std::size_t GetPosition() const = 0;
};

template <class T>
class SequenceReadStream : public ReadOnlyStream<T> {
private:
    std::shared_ptr<Sequence<T>> source_;
    std::unique_ptr<IEnumerator<T>> enumerator_;
    std::size_t position_;
    bool opened_;

public:
    explicit SequenceReadStream(Sequence<T>& source)
        : SequenceReadStream(StreamDetail::MakeNonOwningShared(source)) {
    }

    explicit SequenceReadStream(std::shared_ptr<Sequence<T>> source)
        : source_(StreamDetail::RequireShared(std::move(source), "SequenceReadStream source is null")),
          enumerator_(nullptr), position_(0), opened_(false) {
    }

    void Open() override {
        enumerator_ = std::unique_ptr<IEnumerator<T>>(source_->GetEnumerator());
        position_ = 0;
        opened_ = true;
    }

    void Close() override {
        enumerator_.reset();
        position_ = 0;
        opened_ = false;
    }

    bool IsEndOfStream() const override {
        return position_ >= source_->GetLength();
    }

    T Read() override {
        EnsureOpened();
        if (IsEndOfStream() || !enumerator_->MoveNext()) {
            throw EndOfStream("SequenceReadStream reached end");
        }
        ++position_;
        return enumerator_->Current();
    }

    std::size_t GetPosition() const override {
        return position_;
    }

    bool IsCanSeek() const override {
        return true;
    }

    std::size_t Seek(std::size_t index) override {
        EnsureOpened();
        if (index > source_->GetLength()) {
            throw EndOfStream("Seek index is out of range");
        }

        enumerator_ = std::unique_ptr<IEnumerator<T>>(source_->GetEnumerator());
        position_ = 0;
        while (position_ < index) {
            if (!enumerator_->MoveNext()) {
                throw EndOfStream("Seek index is out of range");
            }
            ++position_;
        }
        return position_;
    }

    bool IsCanGoBack() const override {
        return true;
    }

private:
    void EnsureOpened() const {
        if (!opened_) {
            throw StreamException("Stream is not opened");
        }
    }
};

template <class T>
class LazySequenceReadStream : public ReadOnlyStream<T> {
private:
    std::shared_ptr<LazySequence<T>> source_;
    std::size_t position_;
    bool opened_;

public:
    explicit LazySequenceReadStream(LazySequence<T>& source)
        : LazySequenceReadStream(StreamDetail::MakeNonOwningShared(source)) {
    }

    explicit LazySequenceReadStream(std::shared_ptr<LazySequence<T>> source)
        : source_(StreamDetail::RequireShared(std::move(source), "LazySequenceReadStream source is null")),
          position_(0), opened_(false) {
    }

    void Open() override {
        position_ = 0;
        opened_ = true;
    }

    void Close() override {
        position_ = 0;
        opened_ = false;
    }

    bool IsEndOfStream() const override {
        Ordinal length = source_->GetLength();
        return length.IsFinite() && position_ >= length.FinitePart();
    }

    T Read() override {
        EnsureOpened();
        if (IsEndOfStream()) {
            throw EndOfStream("LazySequenceReadStream reached end");
        }
        T value = source_->Get(Ordinal::Finite(position_));
        ++position_;
        return value;
    }

    std::size_t GetPosition() const override {
        return position_;
    }

    bool IsCanSeek() const override {
        return true;
    }

    std::size_t Seek(std::size_t index) override {
        EnsureOpened();
        Ordinal length = source_->GetLength();
        if (length.IsFinite() && index > length.FinitePart()) {
            throw EndOfStream("Seek index is out of range");
        }
        position_ = index;
        return position_;
    }

    bool IsCanGoBack() const override {
        return true;
    }

private:
    void EnsureOpened() const {
        if (!opened_) {
            throw StreamException("Stream is not opened");
        }
    }
};

template <class T>
class StringReadStream : public ReadOnlyStream<T> {
private:
    std::string source_;
    std::function<T(const std::string&)> deserializer_;
    std::istringstream input_;
    std::size_t position_;
    bool opened_;
    bool endReached_;

public:
    StringReadStream(std::string source, std::function<T(const std::string&)> deserializer)
        : source_(std::move(source)), deserializer_(std::move(deserializer)), input_(), position_(0),
          opened_(false), endReached_(false) {
        if (!deserializer_) {
            throw std::invalid_argument("Deserializer is empty");
        }
    }

    void Open() override {
        input_.clear();
        input_.str(source_);
        position_ = 0;
        opened_ = true;
        endReached_ = false;
    }

    void Close() override {
        input_.clear();
        input_.str("");
        position_ = 0;
        opened_ = false;
        endReached_ = false;
    }

    bool IsEndOfStream() const override {
        return endReached_;
    }

    T Read() override {
        EnsureOpened();
        std::string token;
        if (!(input_ >> token)) {
            endReached_ = true;
            throw EndOfStream("StringReadStream reached end");
        }
        ++position_;
        return deserializer_(token);
    }

    std::size_t GetPosition() const override {
        return position_;
    }

    bool IsCanSeek() const override {
        return true;
    }

    std::size_t Seek(std::size_t index) override {
        EnsureOpened();
        input_.clear();
        input_.str(source_);
        position_ = 0;
        endReached_ = false;

        std::string ignored;
        while (position_ < index) {
            if (!(input_ >> ignored)) {
                endReached_ = true;
                throw EndOfStream("Seek index is out of string range");
            }
            ++position_;
        }
        return position_;
    }

    bool IsCanGoBack() const override {
        return true;
    }

private:
    void EnsureOpened() const {
        if (!opened_) {
            throw StreamException("Stream is not opened");
        }
    }
};

template <class T>
class FileReadStream : public ReadOnlyStream<T> {
private:
    std::string filename_;
    std::function<T(const std::string&)> deserializer_;
    std::ifstream input_;
    std::size_t position_;
    bool opened_;
    bool endReached_;

public:
    FileReadStream(std::string filename, std::function<T(const std::string&)> deserializer)
        : filename_(std::move(filename)), deserializer_(std::move(deserializer)), input_(), position_(0),
          opened_(false), endReached_(false) {
        if (!deserializer_) {
            throw std::invalid_argument("Deserializer is empty");
        }
    }

    void Open() override {
        Close();
        input_.open(filename_);
        if (!input_.is_open()) {
            throw StreamException("Cannot open file for reading: " + filename_);
        }
        position_ = 0;
        opened_ = true;
        endReached_ = false;
    }

    void Close() override {
        if (input_.is_open()) {
            input_.close();
        }
        position_ = 0;
        opened_ = false;
        endReached_ = false;
    }

    bool IsEndOfStream() const override {
        return endReached_;
    }

    T Read() override {
        EnsureOpened();
        std::string line;
        if (!std::getline(input_, line)) {
            endReached_ = true;
            throw EndOfStream("FileReadStream reached end");
        }
        ++position_;
        return deserializer_(line);
    }

    std::size_t GetPosition() const override {
        return position_;
    }

    bool IsCanSeek() const override {
        return true;
    }

    std::size_t Seek(std::size_t index) override {
        EnsureOpened();
        input_.clear();
        input_.seekg(0, std::ios::beg);
        if (!input_) {
            throw StreamException("Cannot seek file: " + filename_);
        }

        position_ = 0;
        endReached_ = false;
        std::string ignored;
        while (position_ < index) {
            if (!std::getline(input_, ignored)) {
                endReached_ = true;
                throw EndOfStream("Seek index is out of file range");
            }
            ++position_;
        }
        return position_;
    }

    bool IsCanGoBack() const override {
        return true;
    }

private:
    void EnsureOpened() const {
        if (!opened_) {
            throw StreamException("Stream is not opened");
        }
    }
};

template <class T>
class SequenceWriteStream : public WriteOnlyStream<T> {
private:
    std::shared_ptr<Sequence<T>> destination_;
    std::size_t position_;
    bool opened_;

public:
    explicit SequenceWriteStream(Sequence<T>& destination)
        : SequenceWriteStream(StreamDetail::MakeNonOwningShared(destination)) {
    }

    explicit SequenceWriteStream(std::shared_ptr<Sequence<T>> destination)
        : destination_(StreamDetail::RequireShared(std::move(destination), "SequenceWriteStream destination is null")),
          position_(0), opened_(false) {
    }

    void Open() override {
        position_ = 0;
        opened_ = true;
    }

    void Close() override {
        position_ = 0;
        opened_ = false;
    }

    std::size_t Write(const T& item) override {
        EnsureOpened();
        std::unique_ptr<Sequence<T>> result(destination_->Append(item));
        if (result.get() != destination_.get()) {
            destination_ = std::shared_ptr<Sequence<T>>(std::move(result));
        } else {
            result.release();
        }
        ++position_;
        return position_;
    }

    std::size_t GetPosition() const override {
        return position_;
    }

private:
    void EnsureOpened() const {
        if (!opened_) {
            throw StreamException("Stream is not opened");
        }
    }
};

template <class T>
class FileWriteStream : public WriteOnlyStream<T> {
private:
    std::string filename_;
    std::function<std::string(const T&)> serializer_;
    std::ofstream output_;
    std::size_t position_;
    bool opened_;
    bool appendMode_;

public:
    FileWriteStream(std::string filename, std::function<std::string(const T&)> serializer,
                    bool appendMode = false)
        : filename_(std::move(filename)), serializer_(std::move(serializer)), output_(), position_(0),
          opened_(false), appendMode_(appendMode) {
        if (!serializer_) {
            throw std::invalid_argument("Serializer is empty");
        }
    }

    void Open() override {
        Close();
        std::ios::openmode mode = std::ios::out;
        if (appendMode_) {
            mode |= std::ios::app;
        }
        output_.open(filename_, mode);
        if (!output_.is_open()) {
            throw StreamException("Cannot open file for writing: " + filename_);
        }
        position_ = 0;
        opened_ = true;
    }

    void Close() override {
        if (output_.is_open()) {
            output_.close();
        }
        position_ = 0;
        opened_ = false;
    }

    std::size_t Write(const T& item) override {
        EnsureOpened();
        output_ << serializer_(item) << '\n';
        if (!output_) {
            throw StreamException("Cannot write to file: " + filename_);
        }
        ++position_;
        return position_;
    }

    std::size_t GetPosition() const override {
        return position_;
    }

private:
    void EnsureOpened() const {
        if (!opened_) {
            throw StreamException("Stream is not opened");
        }
    }
};

inline int DeserializeInt(const std::string& text) {
    return std::stoi(text);
}

inline std::string SerializeInt(const int& value) {
    return std::to_string(value);
}

inline double DeserializeDouble(const std::string& text) {
    return std::stod(text);
}

inline std::string SerializeDouble(const double& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

#endif
