#ifndef RUNPANEL_H
#define RUNPANEL_H

#include "runtime/scenariorunner.h"

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QSpinBox;

namespace nodeeditor {

// Dock contents for a run: the verdict, the elapsed time and the log the
// runner produces. Attach it to a ScenarioRunner and it follows along.
class RunPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RunPanel(QWidget *parent = nullptr);

    void attach(testbuilder::ScenarioRunner *runner);
    void clear();

    // Pace of the run, in ms between steps -- the user's setting, so MainWindow
    // can push it into the runner before starting.
    int stepDelayMs() const;

private:
    void appendEntry(const testbuilder::ScenarioRunner::LogEntry &entry);
    void showVerdict(testbuilder::ScenarioRunner::Verdict verdict, const QString &reason);
    void showState(testbuilder::ScenarioRunner::State state);

    testbuilder::ScenarioRunner *m_runner = nullptr;
    QLabel *m_status = nullptr;
    QSpinBox *m_stepDelay = nullptr;
    QPlainTextEdit *m_log = nullptr;
};

} // namespace nodeeditor

#endif // RUNPANEL_H
