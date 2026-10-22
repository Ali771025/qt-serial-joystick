#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QButtonGroup>
#include <QSerialPortInfo>
#include <QSettings>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QDateTime>
#include <QStyle>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QResizeEvent>
#include <QtMath>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), m_master(this)
{
    ui->setupUi(this);
    auto *modes = new QButtonGroup(this);
    modes->setExclusive(true); modes->addButton(ui->mode1Button); modes->addButton(ui->mode2Button);
    ui->baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    ui->baudCombo->setEditable(true);
    QPalette baudPalette = ui->baudCombo->palette();
    baudPalette.setColor(QPalette::Base, QColor("#0b1626"));
    baudPalette.setColor(QPalette::Text, QColor("#e4ecf7"));
    baudPalette.setColor(QPalette::Highlight, QColor("#277e77"));
    baudPalette.setColor(QPalette::HighlightedText, Qt::white);
    ui->baudCombo->setPalette(baudPalette);
    ui->baudCombo->lineEdit()->setPalette(baudPalette);
    QSettings settings;
    m_savedPort = settings.value("serial/port").toString();
    ui->baudCombo->setCurrentText(settings.value("serial/baud", "115200").toString());
    ui->deviceSpin->setValue(settings.value("serial/device", 0).toInt());
    ui->intervalSpin->setValue(settings.value("serial/intervalMs", 40).toInt());
    const auto updateInterval = [this](int interval) {
        m_master.setIntervalMs(interval);
        ui->subtitleLabel->setText(QString("Serial motion control / ID 1 / %1 ms cycle · %2 Hz")
                                  .arg(interval).arg(1000.0 / interval, 0, 'g', 5));
        ui->intervalSpin->setToolTip(QString("5–1000 ms. Minimum baud for this interval: %1. Set while stopped.")
                                    .arg(SerialMaster::minimumBaud(interval)));
    };
    connect(ui->intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, updateInterval);
    updateInterval(ui->intervalSpin->value());
    refreshPorts();
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(ui->mode1Button, &QRadioButton::clicked, this, [this] { selectMode(1); });
    connect(ui->mode2Button, &QRadioButton::clicked, this, [this] { selectMode(2); });
    connect(ui->joystick, &JoystickWidget::valuesChanged, this, [this](int x, int y) {
        const QSignalBlocker bx(ui->velocityX), by(ui->velocityY);
        ui->velocityX->setValue(x); ui->velocityY->setValue(y); m_master.setVelocity(x, y);
    });
    const auto velocityEdited = [this] {
        ui->joystick->setValues(ui->velocityX->value(), ui->velocityY->value());
        // Also synchronize after clamping if the joystick's value did not change.
        const QPoint p = ui->joystick->values();
        const QSignalBlocker bx(ui->velocityX), by(ui->velocityY);
        ui->velocityX->setValue(p.x()); ui->velocityY->setValue(p.y());
        m_master.setVelocity(p.x(), p.y());
    };
    connect(ui->velocityX, QOverload<int>::of(&QSpinBox::valueChanged), this, velocityEdited);
    connect(ui->velocityY, QOverload<int>::of(&QSpinBox::valueChanged), this, velocityEdited);
    connect(ui->centerButton, &QPushButton::clicked, this, [this] { ui->joystick->setValues(0, 0); });
    connect(ui->positionX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::positionEdited);
    connect(ui->positionY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::positionEdited);
    connect(ui->unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { setDisplayUnit(index == 1); });
    ui->unitCombo->setCurrentIndex(settings.value("ui/angleMode", true).toBool() ? 1 : 0);
    setDisplayUnit(ui->unitCombo->currentIndex() == 1);
    connect(ui->homingButton, &QPushButton::toggled, this, [this](bool enabled) {
        m_master.setHoming(enabled);
        ui->homingButton->setText(enabled ? "Homing / ON" : "Homing / OFF");
        ui->eventLabel->setText(enabled ? "Homing ON: reserve[1] = 1 until clicked again."
                                      : "Homing OFF: reserve[1] = 0.");
    });
    connect(ui->trigButton, &QPushButton::toggled, this, [this](bool enabled) {
        m_master.setTrig(enabled);
        ui->trigButton->setText(enabled ? "trig / ON" : "trig / OFF");
        ui->eventLabel->setText(enabled ? "trig ON: reserve[2] = 1 until clicked again."
                                      : "trig OFF: reserve[2] = 0.");
    });
    ui->reserveValueEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("(?:[0-9]{1,2}|[01][0-9]{2}|2[0-4][0-9]|25[0-5])"), ui->reserveValueEdit));
    connect(ui->reserveValueEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty()) m_master.setReserveValue(0);
        else if (ui->reserveValueEdit->hasAcceptableInput()) m_master.setReserveValue(text.toInt());
    });
    connect(ui->reserveValueEdit, &QLineEdit::editingFinished, this, [this] {
        if (ui->reserveValueEdit->text().isEmpty()) ui->reserveValueEdit->setText("0");
    });
    connect(ui->runButton, &QPushButton::clicked, this, [this] {
        if (m_master.isRunning()) { m_master.stop(); return; }
        bool ok = false; const int baud = ui->baudCombo->currentText().toInt(&ok);
        const int requiredBaud = SerialMaster::minimumBaud(m_master.intervalMs());
        if (!ok || baud < requiredBaud) {
            QMessageBox::warning(this, "Baud rate", QString("Enter a baud rate of at least %1 for a 33-byte / %2 ms cycle.")
                                 .arg(requiredBaud).arg(m_master.intervalMs())); return;
        }
        const QString port = ui->portCombo->currentData().toString();
        if (port.isEmpty()) { refreshPorts(); return; }
        m_hasFeedback = false; updateFeedback();
        m_lastTx.clear(); m_lastRx.clear();
        saveSettings();
        m_master.start(port, baud, quint8(ui->deviceSpin->value()));
    });
    connect(&m_master, &SerialMaster::runningChanged, this, &MainWindow::setRunning);
    connect(&m_master, &SerialMaster::received, this, [this](quint16 x, quint16 y) {
        m_feedbackX = x; m_feedbackY = y; m_hasFeedback = true; updateFeedback();
    });
    connect(&m_master, &SerialMaster::frameSent, this, [this](const QByteArray &b) { m_lastTx = b; });
    connect(&m_master, &SerialMaster::frameReceived, this, [this](const QByteArray &b) { m_lastRx = b; });
    connect(&m_master, &SerialMaster::error, this, [this](const QString &message) {
        ui->eventLabel->setText(message);
        QMessageBox::warning(this, "Serial communication", message);
    });
    m_scanTimer.setInterval(1500);
    connect(&m_scanTimer, &QTimer::timeout, this, &MainWindow::refreshPorts); m_scanTimer.start();
    m_statusTimer.setInterval(100);
    connect(&m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatus); m_statusTimer.start();
    selectMode(1); setRunning(false);
    ui->portCombo->setMinimumWidth(0);
    ui->portCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    ui->portCombo->setMinimumContentsLength(12);
    ui->scrollArea->viewport()->installEventFilter(this);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QSize available = screen->availableGeometry().size() - QSize(48, 64);
        resize(qMax(minimumWidth(), qMin(1060, available.width())),
               qMax(minimumHeight(), qMin(960, available.height())));
    }
    adaptLayout(ui->scrollArea->viewport()->width());
}
MainWindow::~MainWindow() { saveSettings(); m_master.stop(); delete ui; }
void MainWindow::saveSettings()
{
    QSettings settings;
    const QString port = ui->portCombo->currentData().toString();
    if (!port.isEmpty()) settings.setValue("serial/port", port);
    settings.setValue("serial/baud", ui->baudCombo->currentText());
    settings.setValue("serial/device", ui->deviceSpin->value());
    settings.setValue("serial/intervalMs", ui->intervalSpin->value());
    settings.remove("serial/order");
    settings.setValue("ui/angleMode", m_angleMode);
}
void MainWindow::setDisplayUnit(bool angle)
{
    m_angleMode = angle;
    updatePositionInputs();
    updateFeedback();
    ui->positionXLabel->setText(angle ? "X angle (°)" : "X command");
    ui->positionYLabel->setText(angle ? "Y angle (°)" : "Y command");
    ui->hint2->setText(angle ? "Enter an angle: 0 … 511.9921875°. Resolution: 1/128°."
                            : "Set the commanded position. Both axes: 0 … 65535.");
    ui->unitHint->setText(angle ? "Feedback ÷ 128 · Mode 2 commands × 128"
                               : "Feedback and Mode 2 commands use raw numbers");
    const QString title = angle ? "ACTUAL POSITION / ANGLE (°)" : "ACTUAL POSITION / NUMBER";
    ui->feedback1Label->setText(title); ui->feedback2Label->setText(title);
}
void MainWindow::updatePositionInputs()
{
    const QSignalBlocker bx(ui->positionX), by(ui->positionY);
    for (QDoubleSpinBox *input : {ui->positionX, ui->positionY}) {
        input->setDecimals(m_angleMode ? 7 : 0);
        input->setRange(0, m_angleMode ? 65535.0 / 128.0 : 65535.0);
        input->setSuffix(m_angleMode ? "°" : "");
        input->setSingleStep(1);
    }
    const double scale = m_angleMode ? 128.0 : 1.0;
    ui->positionX->setValue(m_positionX / scale);
    ui->positionY->setValue(m_positionY / scale);
}
void MainWindow::positionEdited()
{
    const double scale = m_angleMode ? 128.0 : 1.0;
    m_positionX = qBound(0, qRound(ui->positionX->value() * scale), 65535);
    m_positionY = qBound(0, qRound(ui->positionY->value() * scale), 65535);
    m_master.setPosition(m_positionX, m_positionY);
    updatePositionInputs();
}
void MainWindow::updateFeedback()
{
    const auto format = [this](quint16 raw) {
        if (!m_hasFeedback) return QString("—");
        if (!m_angleMode) return QString::number(raw);
        QString value = QString::number(raw / 128.0, 'f', 7);
        while (value.endsWith('0')) value.chop(1);
        if (value.endsWith('.')) value.chop(1);
        return value + "°";
    };
    ui->rx1X->setText(format(m_feedbackX)); ui->rx2X->setText(format(m_feedbackX));
    ui->rx1Y->setText(format(m_feedbackY)); ui->rx2Y->setText(format(m_feedbackY));
}
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->scrollArea->viewport() && event->type() == QEvent::Resize)
        adaptLayout(static_cast<QResizeEvent*>(event)->size().width());
    return QMainWindow::eventFilter(watched, event);
}
void MainWindow::adaptLayout(int width)
{
    const int category = width < 580 ? 2 : (width < 1000 ? 1 : 0);
    if (category == m_layoutCategory) return;
    m_layoutCategory = category;
    const bool compact = category == 2, stacked = category != 0;
    ui->mainLayout->setContentsMargins(compact ? 12 : 26, 22, compact ? 12 : 26, 22);
    ui->modesLayout->setDirection(stacked ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    ui->velocityLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    ui->headerLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    ui->unitLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    ui->feedback1Layout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    ui->feedback2Layout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    ui->mode2Group->setMinimumHeight(stacked ? 0 : 400);

    // Reuse the Designer widgets while moving controls into rows on narrow screens.
    const auto clearGrid = [](QGridLayout *layout) {
        while (QLayoutItem *item = layout->takeAt(0)) delete item;
    };
    clearGrid(ui->serialLayout);
    if (stacked) {
        ui->serialLayout->addWidget(ui->portLabel, 0, 0, 1, 2);
        ui->serialLayout->addWidget(ui->portCombo, 1, 0); ui->serialLayout->addWidget(ui->refreshButton, 1, 1);
        ui->serialLayout->addWidget(ui->baudLabel, 2, 0); ui->serialLayout->addWidget(ui->deviceLabel, 2, 1);
        ui->serialLayout->addWidget(ui->baudCombo, 3, 0); ui->serialLayout->addWidget(ui->deviceSpin, 3, 1);
        ui->serialLayout->addWidget(ui->orderLabel, 4, 0); ui->serialLayout->addWidget(ui->byteOrderValue, 4, 1);
        ui->serialLayout->addWidget(ui->intervalLabel, 5, 0); ui->serialLayout->addWidget(ui->intervalSpin, 5, 1);
    } else {
        ui->serialLayout->addWidget(ui->portLabel, 0, 0); ui->serialLayout->addWidget(ui->portCombo, 1, 0);
        ui->serialLayout->addWidget(ui->refreshButton, 1, 1);
        ui->serialLayout->addWidget(ui->baudLabel, 0, 2); ui->serialLayout->addWidget(ui->baudCombo, 1, 2);
        ui->serialLayout->addWidget(ui->deviceLabel, 0, 3); ui->serialLayout->addWidget(ui->deviceSpin, 1, 3);
        ui->serialLayout->addWidget(ui->orderLabel, 0, 4); ui->serialLayout->addWidget(ui->byteOrderValue, 1, 4);
        ui->serialLayout->addWidget(ui->intervalLabel, 2, 0); ui->serialLayout->addWidget(ui->intervalSpin, 2, 1, 1, 2);
    }
    clearGrid(ui->positionLayout);
    ui->positionLayout->addWidget(ui->positionXLabel, 0, 0); ui->positionLayout->addWidget(ui->positionX, 1, 0);
    ui->positionLayout->addWidget(ui->positionYLabel, compact ? 2 : 0, compact ? 0 : 1);
    ui->positionLayout->addWidget(ui->positionY, compact ? 3 : 1, compact ? 0 : 1);

    clearGrid(ui->actionsLayout);
    if (compact) {
        ui->actionsLayout->addWidget(ui->modeBadge, 0, 0, 1, 2);
        ui->actionsLayout->addWidget(ui->homingButton, 1, 0); ui->actionsLayout->addWidget(ui->trigButton, 1, 1);
        ui->actionsLayout->addWidget(ui->reserveValueLabel, 2, 0); ui->actionsLayout->addWidget(ui->reserveValueEdit, 2, 1);
        ui->actionsLayout->addWidget(ui->runButton, 3, 0, 1, 2);
    } else if (stacked) {
        ui->actionsLayout->addWidget(ui->modeBadge, 0, 0, 1, 4);
        ui->actionsLayout->addWidget(ui->homingButton, 1, 0); ui->actionsLayout->addWidget(ui->reserveValueLabel, 1, 1);
        ui->actionsLayout->addWidget(ui->reserveValueEdit, 1, 2); ui->actionsLayout->addWidget(ui->trigButton, 1, 3);
        ui->actionsLayout->addWidget(ui->runButton, 2, 0, 1, 4);
    } else {
        ui->actionsLayout->addWidget(ui->modeBadge, 0, 0); ui->actionsLayout->addWidget(ui->homingButton, 0, 1);
        ui->actionsLayout->addWidget(ui->reserveValueLabel, 0, 2); ui->actionsLayout->addWidget(ui->reserveValueEdit, 0, 3);
        ui->actionsLayout->addWidget(ui->trigButton, 0, 4); ui->actionsLayout->addWidget(ui->runButton, 0, 5);
    }
    clearGrid(ui->statsLayout);
    const QList<QWidget*> stats = {ui->txLabel, ui->txCount, ui->rxLabel, ui->rxCount,
                                  ui->badLabel, ui->badCount, ui->ageTitle, ui->ageLabel};
    const int columns = compact ? 2 : (stacked ? 4 : 8);
    for (int i = 0; i < stats.size(); ++i) ui->statsLayout->addWidget(stats[i], i / columns, i % columns);
}
void MainWindow::refreshPorts()
{
    if (m_master.isRunning()) return;
    QString selected = ui->portCombo->currentData().toString();
    if (selected.isEmpty()) selected = m_savedPort;
    auto ports = QSerialPortInfo::availablePorts();
    std::sort(ports.begin(), ports.end(), [](const QSerialPortInfo &a, const QSerialPortInfo &b) {
        return a.portName().localeAwareCompare(b.portName()) < 0;
    });
    QStringList labels, names;
    for (const QSerialPortInfo &p : ports) {
        const QString description = p.description().isEmpty() ? "Serial port" : p.description();
        labels.append(p.portName() + "  ·  " + description); names.append(p.portName());
    }
    if (ports.isEmpty()) { labels.append("No serial ports detected"); names.append(QString()); }
    bool unchanged = ui->portCombo->count() == labels.size();
    for (int i = 0; unchanged && i < labels.size(); ++i)
        unchanged = ui->portCombo->itemText(i) == labels[i] && ui->portCombo->itemData(i).toString() == names[i];
    ui->runButton->setEnabled(!ports.isEmpty());
    if (unchanged) return;
    const QSignalBlocker blocker(ui->portCombo);
    ui->portCombo->clear();
    for (int i = 0; i < labels.size(); ++i) ui->portCombo->addItem(labels[i], names[i]);
    const int index = ui->portCombo->findData(selected);
    if (index >= 0) ui->portCombo->setCurrentIndex(index);
    if (ports.isEmpty()) {
        ui->eventLabel->setText("Connect a serial adapter. Ports are scanned automatically every 1.5 s.");
    } else if (ui->eventLabel->text().startsWith("Connect a serial adapter")) {
        ui->eventLabel->setText("Port detected. Select a mode, then press Run.");
    }
}
void MainWindow::selectMode(int mode)
{
    if (mode == 2 && m_hasFeedback) {
        m_positionX = m_feedbackX;
        m_positionY = m_feedbackY;
        updatePositionInputs();
        m_master.setPosition(m_positionX, m_positionY);
    }
    ui->mode1Button->setChecked(mode == 1); ui->mode2Button->setChecked(mode == 2);
    ui->velocityControls->setEnabled(mode == 1); ui->positionControls->setEnabled(mode == 2);
    ui->modeBadge->setText(mode == 1 ? "MODE 1 / VELOCITY" : "MODE 2 / POSITION");
    m_master.setMode(mode);
}
void MainWindow::setRunning(bool running)
{
    ui->serialControls->setEnabled(!running);
    ui->runButton->setText(running ? "Stop" : "Run");
    ui->runButton->setProperty("running", running);
    ui->runButton->style()->unpolish(ui->runButton); ui->runButton->style()->polish(ui->runButton);
    ui->eventLabel->setText(running ? QString("Sending to ID 1 every %1 ms. Waiting for controller feedback.").arg(m_master.intervalMs())
                                  : "Stopped. Configure the connection and press Run.");
    if (!running) refreshPorts();
    updateStatus();
}
void MainWindow::updateStatus()
{
    ui->txCount->setText(QString::number(m_master.txCount()));
    ui->rxCount->setText(QString::number(m_master.rxCount()));
    ui->badCount->setText(QString::number(m_master.badCount()));
    const qint64 age = m_master.responseAge();
    const int responseTimeout = qMax(500, m_master.intervalMs() * 3);
    ui->ageLabel->setText(age < 0 ? "—" : QString::number(age) + " ms");
    QString status = "STOPPED", color = "#8aa0b9";
    if (m_master.isRunning()) {
        status = age >= 0 && age < responseTimeout ? "ONLINE" : "WAITING FOR RX";
        color = age >= 0 && age < responseTimeout ? "#4cdbba" : "#ffc66d";
    }
    ui->linkStatus->setText("●  " + status); ui->linkStatus->setStyleSheet("color: " + color + ";");
    const bool stale = !m_master.isRunning() || age < 0 || age >= responseTimeout;
    for (QLineEdit *edit : {ui->rx1X, ui->rx1Y, ui->rx2X, ui->rx2Y})
        edit->setStyleSheet(stale ? "color: #8aa0b9;" : "color: #4cdbba;");
    ui->txHex->setText(m_lastTx.isEmpty() ? "—" : QString::fromLatin1(m_lastTx.toHex(' ').toUpper()));
    ui->rxHex->setText(m_lastRx.isEmpty() ? "—" : QString::fromLatin1(m_lastRx.toHex(' ').toUpper()));
}
