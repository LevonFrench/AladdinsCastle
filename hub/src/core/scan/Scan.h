// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/catalog/CatalogLoader.h"
#include <QMap>
#include <atomic>
#include <functional>
namespace ac::scan {
struct ArchiveEntry {
  QString name;
  quint32 crc = 0;
  quint64 size = 0;
  bool hasCrc = false;
};
struct FileIdentity {
  QString path, kind, identity, error, chdHeaderSha1;
  QVector<ArchiveEntry> entries;
  qint64 size = 0, mtime = 0;
  bool cacheHit = false;
  bool chdHeaderBoundsOk = false;
};
struct SupportEntry {
  QString sourceName, targetName;
  quint32 crc32 = 0;
  quint64 size = 0;
};
struct SupportRequirement {
  QString set, sourcePath;
  QVector<SupportEntry> entries;
};
struct Binding {
  QString gameId, requirementId, path, proof, identity;
  QStringList supportPaths, missing;
  QVector<SupportRequirement> supportRequirements;
  bool verified = false;
  QStringList setCandidates; // Metadata-declared clone family, never archive basenames.
  QString bios; // Selected machine BIOS, persisted for the MAME -bios option.
};
struct ToolBinding {
  QString id, path, version;
  bool verified = false;
  qint64 size = 0, mtime = 0;
};
struct ScanOptions {
  QStringList mediaRoots, toolRoots, artRoots;
  QString userRoot, mameXml, supermodelXml, serialIndex;
  int maxDepth = 6;
  bool locateOnly = false;
};
struct ScanResult {
  qint64 elapsedMs = 0;
  QVector<Binding> bindings;
  QVector<ToolBinding> tools;
  QVector<RuntimeState> states;
  QVector<FileIdentity> files;
  QStringList diagnostics;
  bool cancelled = false;
  Json toJson() const;
};
// String/receipt metadata only: never inspects media. Legacy CHD bindings
// require a fresh scan that records bounded physical header provenance.
bool hasValidatedChdBounds(const Json &binding, const Json &files = Json::array());
using Progress = std::function<void(const QVariantMap &)>;
class Scanner {
public:
  static FileIdentity inspect(const QString &path, std::atomic_bool &cancel);
  static ScanResult run(const CatalogData &catalog, const ScanOptions &options,
                        std::atomic_bool &cancel,
                        const Progress &progress = {});
  static ScanOptions optionsFromJson(const Json &json);
  static QString requirementId(const GameRecord &game, const Json &media,
                               int index);
};
} // namespace ac::scan
Q_DECLARE_METATYPE(ac::scan::ScanResult)
