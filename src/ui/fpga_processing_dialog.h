#pragma once
#include <QDialog>
#include <QFutureWatcher>
#include <memory>
#include <atomic>
#include <string>
#include "fpga_processing.h"
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QComboBox;
class QTableView;
class QCustomPlot;
class QAbstractItemModel;
class FpgaProcessingDialog : public QDialog {
public:
    FpgaProcessingDialog(std::shared_ptr<const std::vector<double>> signal, double fs, QWidget *parent = nullptr);
    ~FpgaProcessingDialog() override;
private:
    void start();
    void showResults();
    void showWaveform(int row = -1);
    void showSpectra();
    std::shared_ptr<const std::vector<double>> m_signal;
    double m_fs;
    std::shared_ptr<Fpga::Result> m_result;
    std::shared_ptr<std::atomic_bool> m_cancel = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<std::atomic_int> m_progress = std::make_shared<std::atomic_int>(0);
    std::shared_ptr<std::string> m_failure = std::make_shared<std::string>();
    QFutureWatcher<std::shared_ptr<Fpga::Result>> *m_watcher;
    QSpinBox *m_threshold, *m_tail, *m_gate, *m_peakMin, *m_integralMin, *m_peakShift, *m_integralShift;
    QDoubleSpinBox *m_maxLength, *m_hostPeakWidth, *m_hostIntegralWidth, *m_hostPeakMin, *m_hostIntegralMin;
    QCheckBox *m_hold, *m_percent;
    QPushButton *m_run, *m_stop;
    QLabel *m_status, *m_summary, *m_spectrumInfo;
    QProgressBar *m_bar;
    QComboBox *m_spectrumMode;
    QTableView *m_pulses, *m_gates;
    QCustomPlot *m_wave, *m_peakPlot, *m_integralPlot, *m_rates, *m_fractions;
};
