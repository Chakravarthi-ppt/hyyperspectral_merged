#pragma once
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include "PipelineWorker.h"

class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(const QString& title, QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle(title);
        setMinimumWidth(420);
        setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
        statusLabel_ = new QLabel("Starting…", this);
        statusLabel_->setWordWrap(true);
        bar_ = new QProgressBar(this);
        bar_->setRange(0, 100); bar_->setValue(0); bar_->setTextVisible(true);
        bar_->setStyleSheet("QProgressBar{border:1px solid #bbb;border-radius:4px;"
                            "text-align:center;height:22px;}"
                            "QProgressBar::chunk{background:#2980b9;border-radius:3px;}");
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(statusLabel_); layout->addWidget(bar_);
        layout->setSpacing(10); layout->setContentsMargins(16,16,16,16);
    }

    void runTask(PipelineWorker::Task task) {
        auto* worker = new PipelineWorker(this);
        worker->setTask(std::move(task));
        connect(worker, &PipelineWorker::progress, this, [=](int pct, QString msg){
            bar_->setValue(pct); statusLabel_->setText(msg);
        });
        connect(worker, &PipelineWorker::finished, this, [=](bool ok, QString err){
            succeeded_ = ok; errorMsg_ = err;
            bar_->setValue(100);
            statusLabel_->setText(ok ? "Done." : "Failed: " + err);
            worker->deleteLater(); accept();
        });
        worker->start(); exec();
    }

    bool    succeeded()    const { return succeeded_; }
    QString errorMessage() const { return errorMsg_;  }

private:
    QLabel*       statusLabel_;
    QProgressBar* bar_;
    bool          succeeded_ = false;
    QString       errorMsg_;
};
