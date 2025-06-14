#include <iostream>
#include <fstream>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

// Базовый класс для метрик
class Metric {
public:
    virtual ~Metric() = default;
    virtual std::string get_value() = 0;
    virtual void reset() = 0;
    virtual std::string get_type() const = 0;
};

// Шаблонный класс для числовых метрик
template <typename T>
class NumericMetric : public Metric {
public:
    NumericMetric(std::function<std::string(T)> formatter = nullptr)
        : formatter_(formatter) {}

    void add_value(T value) {
        if constexpr (std::is_floating_point_v<T>) {
            double current = value_.load();
            double new_value;
            do {
                new_value = current + static_cast<double>(value);
            } while (!value_.compare_exchange_weak(current, new_value));
        } else {
            value_.fetch_add(value);
        }
    }

    std::string get_value() override {
        T val = value_.load();
        if (formatter_) {
            return formatter_(val);
        }
        return std::to_string(val);
    }

    void reset() override {
        value_.store(0);
    }

    std::string get_type() const override {
        if constexpr (std::is_integral_v<T>) {
            return "integral";
        } else {
            return "floating";
        }
    }

private:
    std::atomic<T> value_{0};
    std::function<std::string(T)> formatter_;
};

// Класс для сбора и записи метрик
class MetricsCollector {
public:
    template <typename T>
    void register_metric(const std::string& name, 
                         std::function<std::string(T)> formatter = nullptr) {
        metrics_[name] = std::make_unique<NumericMetric<T>>(formatter);
    }

    template <typename T>
    void add_value(const std::string& name, T value) {
        auto it = metrics_.find(name);
        if (it != metrics_.end()) {
            if (auto* metric = dynamic_cast<NumericMetric<T>*>(it->second.get())) {
                metric->add_value(value);
            }
        }
    }

    void collect_and_write(const std::string& filename) {
        std::ofstream file(filename, std::ios::app);
        if (!file.is_open()) {
            std::cerr << "Error opening metrics file!" << std::endl;
            return;
        }

        // Получаем текущее время
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        file << "[" << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S") << "]";

        // Собираем и записываем метрики
        for (auto& [name, metric] : metrics_) {
            file << " \"" << name << "\" " << metric->get_value();
            metric->reset();
        }
        file << std::endl;
    }

private:
    std::unordered_map<std::string, std::unique_ptr<Metric>> metrics_;
};

// Класс для периодической записи метрик
class MetricsWriter {
public:
    MetricsWriter(MetricsCollector& collector, 
                  const std::string& filename, 
                  std::chrono::seconds interval = std::chrono::seconds(1))
        : collector_(collector), filename_(filename), interval_(interval), running_(false) {}

    ~MetricsWriter() {
        stop();
    }

    void start() {
        running_ = true;
        writer_thread_ = std::thread([this]() {
            while (running_) {
                auto start = std::chrono::steady_clock::now();
                collector_.collect_and_write(filename_);
                auto end = std::chrono::steady_clock::now();
                auto elapsed = end - start;
                auto sleep_time = interval_ - elapsed;
                if (sleep_time > std::chrono::seconds(0)) {
                    std::this_thread::sleep_for(sleep_time);
                }
            }
        });
    }

    void stop() {
        running_ = false;
        if (writer_thread_.joinable()) {
            writer_thread_.join();
        }
    }

private:
    MetricsCollector& collector_;
    std::string filename_;
    std::chrono::seconds interval_;
    std::atomic<bool> running_;
    std::thread writer_thread_;
};