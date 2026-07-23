#pragma once
#include <QString>
#include <QProgressBar>
#include <QPushButton>
#include <QHBoxLayout>
#include <QDialog>

// Shared visual style for the Hyperspectral dialogs (Preprocessing, PCA,
// Classification, Thematic, Change Detection), matching the INDIGIS TechK
// reference mockups: light grey dialog body, bold navy field labels,
// rounded white input fields, and a bottom row with a percentage progress
// bar + pill-shaped Run / Reset / Cancel buttons.
//
// Usage in a dialog constructor:
//   setStyleSheet(indigisDialogStyleSheet());
//   ...
//   QPushButton *runBtn, *resetBtn, *cancelBtn; QProgressBar* progress;
//   mainLayout->addLayout(buildRunResetCancelRow(this, progress, runBtn, resetBtn, cancelBtn));
//   connect(runBtn, ...); connect(resetBtn, ...);
//   connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
//
// NOTE: the progress bar is cosmetic (fixed at 0%) unless a dialog's run()
// is wired up to advance it -- none of these pipeline calls currently
// report incremental progress, so wiring a real percentage would need
// threading/callback work in the core library, which is out of scope here.
// It's included because the mockups show it, and set up so it's trivial to
// drive for real later (just call progress->setValue(...)).

inline QString indigisDialogStyleSheet() {
    return R"(
        QDialog {
            background: #eef1f4;
        }
        QGroupBox {
            background: #eef1f4;
            border: none;
            margin-top: 6px;
            font-weight: bold;
            color: #0b3d6b;
        }
        QLabel {
            color: #0b3d6b;
            font-weight: bold;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background: white;
            border: 1px solid #c7cdd4;
            border-radius: 6px;
            padding: 6px 10px;
            color: #333;
            font-weight: normal;
        }
        QLineEdit:read-only, QSpinBox:disabled, QDoubleSpinBox:disabled {
            background: #f4f5f6;
            color: #888;
        }
        QProgressBar {
            border: 1px solid #c7cdd4;
            border-radius: 6px;
            background: white;
            text-align: center;
            color: #333;
            min-height: 30px;
        }
        QProgressBar::chunk {
            background-color: #6fa8d8;
            border-radius: 6px;
        }
        QPushButton {
            background: white;
            border: 1px solid #0b3d6b;
            border-radius: 15px;
            padding: 6px 22px;
            color: #0b3d6b;
            font-weight: bold;
        }
        QPushButton:hover {
            background: #e3ecf5;
        }
        QPushButton:pressed {
            background: #cfe0ef;
        }
        QPushButton:default {
            background: #0b3d6b;
            color: white;
        }
        QPushButton:default:hover {
            background: #124c85;
        }
    )";
}

// Builds the bottom "[progress bar] [Run] [Reset] [Cancel]" row shown in
// every reference mockup. Hands back the three buttons and the progress
// bar so the caller can connect signals / drive progress.
inline QHBoxLayout* buildRunResetCancelRow(QWidget* parent,
                                            QProgressBar*& progressOut,
                                            QPushButton*& runOut,
                                            QPushButton*& resetOut,
                                            QPushButton*& cancelOut) {
    progressOut = new QProgressBar(parent);
    progressOut->setRange(0, 100);
    progressOut->setValue(0);
    progressOut->setFormat("%p%");

    runOut    = new QPushButton("Run", parent);
    runOut->setDefault(true);
    resetOut  = new QPushButton("Reset", parent);
    cancelOut = new QPushButton("Cancel", parent);

    auto* row = new QHBoxLayout();
    row->addWidget(progressOut, /*stretch=*/1);
    row->addWidget(runOut);
    row->addWidget(resetOut);
    row->addWidget(cancelOut);
    return row;
}
