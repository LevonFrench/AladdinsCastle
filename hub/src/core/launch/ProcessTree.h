// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QList>
#include <QStringList>
#include <QtGlobal>
namespace ac::launch {
// Every process a launch started, children and grandchildren included, held in one
// Windows job object so the Hub can close or end them together. Elsewhere it holds nothing.
class ProcessTree {
public:
    ProcessTree();
    ~ProcessTree();
    ProcessTree(const ProcessTree &)=delete;ProcessTree &operator=(const ProcessTree &)=delete;
    // Puts a process the Hub started suspended into the job, then lets it run.
    // It always lets it run, even when it could not be put in the job.
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
};
// Processes with one of these program names started after sinceMs (ms since the epoch):
// helpers Windows starts on a game's behalf, outside its tree, such as a backglass server.
QList<qint64> strayHelpers(const QStringList &programNames,qint64 sinceMs);
void closeProcessWindows(const QList<qint64> &pids);
void endProcesses(const QList<qint64> &pids);
bool processAlive(qint64 pid);
}
