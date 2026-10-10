// SPDX-License-Identifier: GPL-3.0-only
#include "ControlsViewModel.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QImageReader>
#include <QRegularExpression>
#include <cmath>

namespace ac {
namespace {
QString pretty(QString value) {
    value.replace('-', ' ');
    value.replace('_', ' ');
    if (!value.isEmpty()) value[0] = value[0].toUpper();
    return value;
}
QString confinedFile(const QString &directory, const QString &basename) {
    if (directory.isEmpty() || basename.isEmpty() || QFileInfo(basename).fileName() != basename ||
        basename.contains('\\') || basename.contains('/') || basename == "." || basename == "..") return {};
    const QString root = QFileInfo(directory).canonicalFilePath();
    const QFileInfo file(QDir(directory).filePath(basename));
    const QString path = file.canonicalFilePath();
    // QFileInfo canonical paths use forward slashes on every Qt platform.
#ifdef Q_OS_WIN
    constexpr auto sensitivity = Qt::CaseInsensitive;
#else
    constexpr auto sensitivity = Qt::CaseSensitive;
#endif
    if (root.isEmpty() || path.isEmpty() || !file.isFile() ||
        !path.startsWith(root + '/', sensitivity)) return {};
    return path;
}
bool coordinates(const QVariant &value) {
    const auto p = value.toList();
    if (p.size() != 2) return false;
    for (const auto &v : p) {
        bool valid = false;
        const double n = v.toDouble(&valid);
        if (!valid || !std::isfinite(n) || n < 0 || n > 1) return false;
    }
    return true;
}
}

ControlsViewModel::ControlsViewModel(QObject *parent) : QObject(parent) {}
QString ControlsViewModel::stateKey(const QString &hand, const QString &control) { return hand + '/' + control; }

bool ControlsViewModel::loadResolvedJson(const QByteArray &json, const QString &directory) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        clear(); m_error = "Controls data could not be read"; emit changed(); return false;
    }
    auto result = document.object().toVariantMap();
    if (result.contains("games")) {
        const auto games = result.value("games").toList();
        if (games.size() != 1 || !result.value("errors").toList().isEmpty()) {
            clear(); m_error = "Choose one game with valid controls data"; emit changed(); return false;
        }
        result = games.front().toMap();
    }
    return loadResolved(result, directory);
}

bool ControlsViewModel::loadResolved(const QVariantMap &result, const QString &directory) {
    clear();
    const auto data = result.value("data").toMap();
    static const QRegularExpression id("^[a-z0-9]+(?:-[a-z0-9]+)*$");
    if (data.value("version").toString() != "0.1" || !id.match(result.value("model").toString()).hasMatch() ||
        result.value("game_id").toString().isEmpty() || data.value("title").toString().isEmpty()) {
        m_error = "Controls data is unavailable"; emit changed(); return false;
    }
    QSet<QString> ids;
    for (const QString key : {QString("element"), QString("unmapped_part")}) {
        for (const auto &value : data.value(key).toList()) {
            const auto row = value.toMap();
            const QString rowId = row.value("id").toString();
            if (rowId.isEmpty() || ids.contains(rowId) || row.value("label").toString().isEmpty()) {
                m_error = "Controls contain an invalid or duplicate part"; emit changed(); return false;
            }
            ids.insert(rowId);
        }
    }
    m_result = result; m_data = data; m_previewDirectory = directory;
    // Backend capacity does not mean player two has joined. The host explicitly
    // sets twoGunsActive when choosing a dual-hand preview or after joining.
    m_twoGunsActive = false;
    loadPreview(); rebuild(); return true;
}

bool ControlsViewModel::loadCatalogGame(const QByteArray &catalog, const QString &gid, const QString &directory) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(catalog,&error);
    if (error.error == QJsonParseError::NoError && document.isObject() &&
        document.object().value("version").toString() == "0.1") {
        const auto games = document.object().value("games").toArray();
        for (const auto &game : games) {
            if (game.toObject().value("game_id").toString() == gid)
                return loadResolved(game.toObject().toVariantMap(),directory);
        }
    }
    clear(); m_error = "No controls data for this game"; emit changed(); return false;
}

void ControlsViewModel::clear() {
    m_result.clear(); m_data.clear(); m_rows.clear(); m_preview.clear(); m_anchors.clear();
    m_previewImage = {}; m_previewDirectory.clear(); m_pressed.clear(); m_error.clear(); m_twoGunsActive = false;
    emit changed();
}

void ControlsViewModel::loadPreview() {
    m_previewImage = {}; m_anchors.clear(); m_preview.clear();
    const QString manifest = confinedFile(m_previewDirectory, modelId() + ".json");
    if (manifest.isEmpty()) return;
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return;
    const auto preview = document.object().toVariantMap();
    const auto size = preview.value("size").toList();
    if (preview.value("version").toString() != "0.1" || preview.value("model").toString() != modelId() ||
        size != QVariantList{512,512}) return;
    const auto view = preview.value("views").toMap().value(m_view).toMap();
    const QString image = confinedFile(m_previewDirectory, view.value("image").toString());
    QImageReader reader(image);
    if (image.isEmpty() || reader.size() != QSize(512,512) || !reader.canRead()) return;
    const auto anchors = view.value("nodes").toMap();
    for (auto it = anchors.begin(); it != anchors.end(); ++it) if (!coordinates(it.value())) return;
    m_preview = preview; m_anchors = anchors; m_previewImage = QUrl::fromLocalFile(image);
}

QString ControlsViewModel::elementHand(const QVariantMap &element, const QVariantMap &binding) const {
    const QString hand = binding.value("hand").toString();
    if (hand != "slot") return hand;
    return element.value("slot").toInt() == 0 ? m_primaryHand : (m_primaryHand == "right" ? "left" : "right");
}
QString ControlsViewModel::bindingLabel(const QVariantMap &binding, const QString &hand) {
    const QString control = binding.value("control").toString();
    const QString side = hand == "either" ? "Either hand" : pretty(hand);
    QString label;
    if (control == "primary") label = hand == "right" ? "A (right)" : hand == "left" ? "X (left)" : "A / X";
    else if (control == "secondary") label = hand == "right" ? "B (right)" : hand == "left" ? "Y (left)" : "B / Y";
    else if (control == "offscreen_trigger") label = side + " trigger, aimed off-screen";
    else if (control == "flick_up") label = side + " wrist flick up";
    else if (control == "pump") label = "Pump with free hand";
    else if (control == "slide") label = "Slide with free hand";
    else if (control == "thumbstick_click") label = side + " thumbstick click";
    else if (control == "menu_chord") label = "Menu chord";
    else if (control == "trigger" || control == "grip") label = side + ' ' + control;
    else return "Binding pending";
    if (binding.value("mode").toString() == "hold") label += " (hold)";
    if (binding.value("mode").toString() == "toggle") label += " (toggle)";
    return label;
}

QVariantList ControlsViewModel::makeRows() const {
    QVariantList result;
    auto add = [this,&result](const QVariantMap &source, bool unmapped) {
        const auto ordinary = source.value("binding").toMap();
        const auto fallback = source.value("fallback_binding").toMap();
        const auto binding = m_twoGunsActive && !fallback.isEmpty() ? fallback : ordinary;
        const QString hand = elementHand(source,binding);
        const QString control = binding.value("control").toString();
        const auto availability = source.value("availability").toMap();
        QString state = availability.value("state").toString();
        if (unmapped) state = "unmapped";
        else if (state != "available" && state != "unavailable") state = "unverified";
        const QString node = source.value("node").toString();
        const auto anchor = m_anchors.value(node).toList();
        const bool pressed = !unmapped && (m_pressed.contains(stateKey(hand,control)) ||
            (hand == "either" && (m_pressed.contains(stateKey("left",control)) || m_pressed.contains(stateKey("right",control)))));
        const QString part = unmapped ? source.value("label").toString() : pretty(source.value("part").toString());
        const QString action = unmapped ? "Game action pending" : source.value("label").toString();
        const QString bindingText = unmapped ? "Binding pending" : bindingLabel(binding,hand);
        const QString availabilityLabel = state == "available" ? "Ready to test" : state == "unavailable" ? "Unavailable for this setup" :
                                          state == "unmapped" ? "Mapping pending" : "Awaiting validation";
        QVariantMap row{{"id",source.value("id")},{"part",part},{"action",action},{"node",node},
            {"slot",source.value("slot")},{"player",source.value("player")},{"hand",hand},{"control",control},
            {"bindingLabel",bindingText},{"fallbackLabel",fallback.isEmpty() ? QString() : bindingLabel(fallback,elementHand(source,fallback))},
            {"availability",state},{"availabilityLabel",availabilityLabel},{"reason",unmapped ? source.value("reason") : availability.value("reason")},
            {"pressed",pressed},{"anchorAvailable",previewAvailable() && coordinates(anchor)},
            {"anchorX",anchor.size()==2 ? anchor[0] : QVariant(0.0)},{"anchorY",anchor.size()==2 ? anchor[1] : QVariant(0.0)},
            {"callout",part + " → " + action + " → " + bindingText}};
        result.append(row);
    };
    for (const auto &row : m_data.value("element").toList()) add(row.toMap(),false);
    for (const auto &row : m_data.value("unmapped_part").toList()) add(row.toMap(),true);
    return result;
}
void ControlsViewModel::rebuild() { m_rows = makeRows(); emit changed(); }
QString ControlsViewModel::notice() const {
    if (!m_error.isEmpty()) return m_error;
    if (m_rows.isEmpty()) return "Choose a game to see its controls";
    for (const auto &value : m_rows) if (value.toMap().value("availability").toString() != "available")
        return "Some controls are awaiting validation. Their mappings are shown for review.";
    return "Controls match the supplied input declarations. Gameplay validation is separate.";
}
void ControlsViewModel::setPreviewView(const QString &view) {
    if ((view != "front" && view != "threequarter") || m_view == view) return;
    m_view = view; loadPreview(); rebuild();
}
void ControlsViewModel::setPrimaryHand(const QString &hand) {
    if ((hand != "left" && hand != "right") || hand == m_primaryHand) return;
    m_primaryHand = hand; m_pressed.clear(); rebuild();
}
void ControlsViewModel::setTwoGunsActive(bool active) {
    if (active == m_twoGunsActive || (active && m_result.value("configured_slots").toInt() < 2)) return;
    m_twoGunsActive = active; m_pressed.clear(); rebuild();
}
void ControlsViewModel::setBindingState(const QString &hand, const QString &control, bool pressed) {
    if (hand != "left" && hand != "right") return;
    const QString key = stateKey(hand,control);
    if (pressed == m_pressed.contains(key)) return;
    if (pressed) m_pressed.insert(key); else m_pressed.remove(key);
    rebuild();
}
void ControlsViewModel::clearBindingStates() { if (!m_pressed.isEmpty()) { m_pressed.clear(); rebuild(); } }

QVariantList ControlsViewModel::controllerBindings(const QString &hand) const {
    QVariantList result;
    for (const auto &value : m_rows) {
        auto row = value.toMap();
        if (row.value("availability").toString() == "unmapped" ||
            (row.value("hand").toString() != hand && row.value("hand").toString() != "either")) continue;
        const QString control = row.value("control").toString();
        QString highlight = control;
        if (highlight == "offscreen_trigger") highlight = "trigger";
        row["controllerControl"] = highlight;
        result.append(row);
    }
    return result;
}
QVariantList ControlsViewModel::markers() const {
    QVariantList result;
    QMap<QString,QVariantMap> nodes;
    int number = 0;
    for (const auto &value : m_rows) {
        ++number;
        const auto row = value.toMap();
        if (!row.value("anchorAvailable").toBool()) continue;
        const QString node = row.value("node").toString();
        auto marker = nodes.value(node);
        if (marker.isEmpty()) marker = {{"node",node},{"x",row.value("anchorX")},{"y",row.value("anchorY")},
                                       {"numbers",QString()},{"callout",QString()},{"pressed",false}};
        marker["numbers"] = marker.value("numbers").toString() + (marker.value("numbers").toString().isEmpty() ? "" : ", ") + QString::number(number);
        marker["callout"] = marker.value("callout").toString() + (marker.value("callout").toString().isEmpty() ? "" : "\n") + row.value("callout").toString();
        marker["pressed"] = marker.value("pressed").toBool() || row.value("pressed").toBool();
        nodes[node] = marker;
    }
    for (const auto &marker : nodes) result.append(marker);
    return result;
}
QUrl ControlsViewModel::controllerImage(const QString &hand) const {
    return QUrl(hand == "left" ? "qrc:/controls/icons/touch-left.svg" : "qrc:/controls/icons/touch-right.svg");
}
}
