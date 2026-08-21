#include "runpanel.h"

#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

using runtime::ScenarioRunner;

namespace nodeeditor {

namespace {

QString levelTag(ScenarioRunner::LogLevel level)
{
    switch (level) {
    case ScenarioRunner::LogLevel::Warning:
        return QStringLiteral("WARN");
    case ScenarioRunner::LogLevel::Error:
        return QStringLiteral("FAIL");
    case ScenarioRunner::LogLevel::Info:
        break;
    }
    return QStringLiteral("INFO");
}

QString verdictText(ScenarioRunner::Verdict verdict)
{
    switch (verdict) {
    case ScenarioRunner::Verdict::Pass:
        return QStringLiteral("PASS");
    case ScenarioRunner::Verdict::Fail:
        return QStringLiteral("FAIL");
    case ScenarioRunner::Verdict::Abort:
        return QStringLiteral("ABORTED");
    case ScenarioRunner::Verdict::Error:
        return QStringLiteral("ERROR");
    case ScenarioRunner::Verdict::None:
        break;
    }
    return QStringLiteral("--");
}

QString verdictColor(ScenarioRunner::Verdict verdict)
{
    switch (verdict) {
    case ScenarioRunner::Verdict::Pass:
        return QStringLiteral("#3fa46a");
    case ScenarioRunner::Verdict::Fail:
    case ScenarioRunner::Verdict::Error:
        return QStringLiteral("#d64b4b");
    case ScenarioRunner::Verdict::Abort:
        return QStringLiteral("#b9770f");
    case ScenarioRunner::Verdict::None:
        break;
    }
    return QStringLiteral("palette(text)");
}

} // namespace

RunPanel::RunPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *topRow = new QHBoxLayout;
    m_status = new QLabel(tr("Idle"), this);
    topRow->addWidget(m_status, 1);

    topRow->addWidget(new QLabel(tr("Step delay"), this));
    m_stepDelay = new QSpinBox(this);
    m_stepDelay->setRange(0, 5000);
    m_stepDelay->setSingleStep(50);
    m_stepDelay->setValue(120);
    m_stepDelay->setSuffix(tr(" ms"));
    m_stepDelay->setToolTip(tr("Pause between steps so a run can be followed on the canvas. "
                               "0 runs at full speed."));
    topRow->addWidget(m_stepDelay);
    layout->addLayout(topRow);

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(m_log, 1);
}

int RunPanel::stepDelayMs() const
{
    return m_stepDelay->value();
}

void RunPanel::attach(runtime::ScenarioRunner *runner)
{
    if (m_runner)
        m_runner->disconnect(this);

    m_runner = runner;
    if (!m_runner)
        return;

    connect(m_runner, &ScenarioRunner::logged, this, &RunPanel::appendEntry);
    connect(m_runner, &ScenarioRunner::finished, this, &RunPanel::showVerdict);
    connect(m_runner, &ScenarioRunner::stateChanged, this, &RunPanel::showState);
    connect(m_runner, &ScenarioRunner::started, this, &RunPanel::clear);
}

void RunPanel::clear()
{
    m_log->clear();
    m_status->setText(tr("Running..."));
    m_status->setStyleSheet(QString());
}

void RunPanel::appendEntry(const ScenarioRunner::LogEntry &entry)
{
    m_log->appendPlainText(QStringLiteral("%1  %2  %3")
                               .arg(entry.elapsedMs, 6)
                               .arg(levelTag(entry.level), -4)
                               .arg(entry.text));
}

void RunPanel::showVerdict(ScenarioRunner::Verdict verdict, const QString &reason)
{
    const qint64 elapsed = m_runner ? m_runner->elapsedMs() : 0;
    m_status->setText(tr("%1 - %2 (%3 ms)").arg(verdictText(verdict), reason).arg(elapsed));
    m_status->setStyleSheet(QStringLiteral("font-weight: bold; color: %1;").arg(verdictColor(verdict)));

    m_log->appendPlainText(QString());
    m_log->appendPlainText(QStringLiteral("%1  %2  %3")
                               .arg(elapsed, 6)
                               .arg(verdictText(verdict), -4)
                               .arg(reason));
}

void RunPanel::showState(ScenarioRunner::State state)
{
    switch (state) {
    case ScenarioRunner::State::Running:
        m_status->setText(tr("Running..."));
        break;
    case ScenarioRunner::State::Waiting:
        m_status->setText(tr("Waiting for the bus..."));
        break;
    case ScenarioRunner::State::Paused:
        m_status->setText(tr("Paused"));
        break;
    case ScenarioRunner::State::Idle:
        m_status->setText(tr("Idle"));
        break;
    case ScenarioRunner::State::Finished:
        break; // showVerdict() has the interesting part
    }
}

} // namespace nodeeditor
