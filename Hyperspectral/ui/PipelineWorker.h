#pragma once
#include <QThread>
#include <functional>
#include <string>

class PipelineWorker : public QThread {
    Q_OBJECT
public:
    explicit PipelineWorker(QObject* parent = nullptr) : QThread(parent) {}
    using Task = std::function<void(PipelineWorker*)>;
    void setTask(Task t) { task_ = std::move(t); }
    void reportProgress(int percent, const QString& message) { emit progress(percent, message); }

signals:
    void progress(int percent, QString message);
    void finished(bool success, QString errorMessage);

protected:
    void run() override {
        try {
            if (task_) task_(this);
            emit finished(true, QString());
        } catch (const std::exception& e) {
            emit finished(false, QString::fromStdString(e.what()));
        } catch (...) {
            emit finished(false, "Unknown exception in pipeline worker.");
        }
    }
private:
    Task task_;
};
