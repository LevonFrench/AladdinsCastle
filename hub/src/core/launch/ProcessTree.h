// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QList>
#include <QStringList>
#include <QtGlobal>
#include <functional>
namespace ac::launch {
// Every process a launch started, children and grandchildren included, held in one
// Windows job object so the Hub can close or end them together. Elsewhere it holds nothing.
class ProcessTree {
public:
    explicit ProcessTree(std::function<bool(qint64)> adoptionGate = {});
    ~ProcessTree();
    ProcessTree(const ProcessTree &)=delete;ProcessTree &operator=(const ProcessTree &)=delete;
    // Puts a process the Hub started suspended into the job, then lets it run.
    // Failure leaves it suspended; the caller must kill its exact QProcess.
    bool adopt(qint64 pid);
    QList<qint64> members()const;
    // Asks every window of the tree to close, the ordinary way.
    void closeWindows()const;
    // Ends every process in the tree at once.
    void end();
    // Closing the Hub, or the Hub ending any other way, ends whatever is left in the tree.
    // Set when the tree is made, so a game never outlives a Hub that crashed.
    void endWithHub();
private:
    void *job_=nullptr;
    std::function<bool(qint64)> adoptionGate_;
};
void closeProcessWindows(const QList<qint64> &pids);
void endProcesses(const QList<qint64> &pids);
bool processAlive(qint64 pid);
}
