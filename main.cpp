int main() {
    MetricsCollector collector;
    
    // Регистрация метрики CPU (среднее значение)
    collector.register_metric<double>("CPU", [](double value) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << value;
        return stream.str();
    });
    
    // Регистрация метрики HTTP запросов (целое число)
    collector.register_metric<int>("HTTP requests RPS");

    // Создаем и запускаем сборщик метрик
    MetricsWriter writer(collector, "metrics.log");
    writer.start();

    // Имитация работы системы
    for (int i = 0; i < 5; ++i) {
        // Генерация тестовых значений
        double cpu_usage = 0.5 + static_cast<double>(rand()) / RAND_MAX * 2.5;
        int http_requests = 30 + rand() % 50;
        
        // Добавление значений в метрики (неблокирующая операция)
        collector.add_value("CPU", cpu_usage);
        collector.add_value("HTTP requests RPS", http_requests);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Даем время на запись последних метрик
    std::this_thread::sleep_for(std::chrono::seconds(2));
    writer.stop();

    return 0;
}